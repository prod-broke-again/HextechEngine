#include "engine/audio/AudioEngine.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/Path.hpp"

#include <miniaudio.h>

#include <filesystem>
#include <mutex>
#include <vector>

namespace engine {

struct Active3DSound {
    ma_sound sound;
    bool inUse = false;
};

struct AudioEngine::Impl {
    ma_engine engine{};
    std::vector<Active3DSound> soundPool;
    std::mutex mutex;
    static constexpr size_t kMaxConcurrent3DSounds = 32;

    bool init() {
        ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            log(LogLevel::Error, "AudioEngine: failed to initialize miniaudio engine");
            return false;
        }

        soundPool.resize(kMaxConcurrent3DSounds);
        for (auto& s : soundPool) {
            s.inUse = false;
        }

        log(LogLevel::Info, "AudioEngine: initialized with miniaudio");
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& s : soundPool) {
            if (s.inUse) {
                ma_sound_stop(&s.sound);
                ma_sound_uninit(&s.sound);
                s.inUse = false;
            }
        }
        ma_engine_uninit(&engine);
        log(LogLevel::Info, "AudioEngine: shutdown");
    }

    void updateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
        ma_engine_listener_set_position(&engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&engine, 0, up.x, up.y, up.z);

        std::lock_guard<std::mutex> lock(mutex);
        for (auto& s : soundPool) {
            if (s.inUse && ma_sound_at_end(&s.sound)) {
                ma_sound_uninit(&s.sound);
                s.inUse = false;
            }
        }
    }

    void play2D(const std::string& soundPath, float volume, float pitch) {
        const auto resolved = resolvePath(soundPath);
        if (!std::filesystem::exists(resolved)) {
            log(LogLevel::Warn, "AudioEngine: sound file not found: " + soundPath);
            return;
        }
        (void)volume;
        (void)pitch;
        ma_engine_play_sound(&engine, resolved.string().c_str(), nullptr);
    }

    void play3D(const std::string& soundPath, const glm::vec3& position, float volume,
                float minDistance, float maxDistance) {
        const auto resolved = resolvePath(soundPath);
        if (!std::filesystem::exists(resolved)) {
            log(LogLevel::Warn, "AudioEngine: sound file not found: " + soundPath);
            return;
        }

        std::lock_guard<std::mutex> lock(mutex);
        Active3DSound* slot = nullptr;
        for (auto& s : soundPool) {
            if (!s.inUse) {
                slot = &s;
                break;
            } else if (ma_sound_at_end(&s.sound)) {
                ma_sound_uninit(&s.sound);
                s.inUse = false;
                slot = &s;
                break;
            }
        }

        if (!slot) {
            slot = &soundPool[0];
            ma_sound_stop(&slot->sound);
            ma_sound_uninit(&slot->sound);
            slot->inUse = false;
        }

        ma_result result = ma_sound_init_from_file(&engine, resolved.string().c_str(),
                                                   MA_SOUND_FLAG_DECODE, nullptr, nullptr, &slot->sound);
        if (result != MA_SUCCESS) {
            log(LogLevel::Warn, "AudioEngine: failed to load 3D sound: " + soundPath);
            return;
        }

        ma_sound_set_position(&slot->sound, position.x, position.y, position.z);
        ma_sound_set_volume(&slot->sound, volume);
        ma_sound_set_min_distance(&slot->sound, minDistance);
        ma_sound_set_max_distance(&slot->sound, maxDistance);
        ma_sound_set_spatialization_enabled(&slot->sound, MA_TRUE);
        ma_sound_set_attenuation_model(&slot->sound, ma_attenuation_model_inverse);
        ma_sound_start(&slot->sound);
        slot->inUse = true;
    }

    void setMasterVolume(float volume) {
        ma_engine_set_volume(&engine, volume);
    }

    float masterVolume() const {
        return ma_engine_get_volume(const_cast<ma_engine*>(&engine));
    }
};

AudioEngine& AudioEngine::instance() {
    static AudioEngine s_instance;
    return s_instance;
}

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    if (m_initialized) {
        shutdown();
    }
}

bool AudioEngine::init() {
    if (m_initialized) return true;
    m_initialized = m_impl->init();
    return m_initialized;
}

void AudioEngine::shutdown() {
    if (!m_initialized) return;
    m_impl->shutdown();
    m_initialized = false;
}

void AudioEngine::updateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    if (!m_initialized) return;
    m_impl->updateListener(position, forward, up);
}

void AudioEngine::play2D(const std::string& soundPath, float volume, float pitch) {
    if (!m_initialized) return;
    m_impl->play2D(soundPath, volume, pitch);
}

void AudioEngine::play3D(const std::string& soundPath, const glm::vec3& position, float volume,
                         float minDistance, float maxDistance) {
    if (!m_initialized) return;
    m_impl->play3D(soundPath, position, volume, minDistance, maxDistance);
}

void AudioEngine::setMasterVolume(float volume) {
    if (!m_initialized) return;
    m_impl->setMasterVolume(volume);
}

float AudioEngine::masterVolume() const {
    if (!m_initialized) return 1.0f;
    return m_impl->masterVolume();
}

} // namespace engine
