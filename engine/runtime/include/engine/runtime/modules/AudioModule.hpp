#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/audio/AudioEngine.hpp"

namespace engine {

class AudioModule : public IModule {
public:
    std::string_view name() const override { return "AudioModule"; }

    void onAttach(World& world) override {
        // AudioEngine is a singleton currently in EraApp.cpp: AudioEngine::instance().init();
        AudioEngine::instance().init();
    }

    void onDetach(World& world) override {
        // AudioEngine has shutdown? Let's assume it cleans up or add it if needed
    }
};

} // namespace engine

