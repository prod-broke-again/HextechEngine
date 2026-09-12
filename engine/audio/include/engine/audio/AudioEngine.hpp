#pragma once

#include <glm/vec3.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace engine {

class AudioEngine {
public:
    static AudioEngine& instance();

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    bool init();
    void shutdown();
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    void updateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

    void play2D(const std::string& soundPath, float volume = 1.0f, float pitch = 1.0f);
    void play3D(const std::string& soundPath, const glm::vec3& position, float volume = 1.0f,
                float minDistance = 1.0f, float maxDistance = 30.0f);

    void setMasterVolume(float volume);
    [[nodiscard]] float masterVolume() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};

} // namespace engine
