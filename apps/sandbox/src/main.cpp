#include "SandboxModule.hpp"
#include "engine/runtime/Engine.hpp"
#include "engine/runtime/modules/PlatformModule.hpp"
#include "engine/runtime/modules/RenderModule.hpp"
#include "engine/runtime/modules/UiModule.hpp"
#include "engine/runtime/modules/VfxModule.hpp"
#include "engine/runtime/modules/AudioModule.hpp"
#include "engine/runtime/modules/PhysicsModule.hpp"
#include <string>

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") {
            smokeTest = true;
        }
    }

    engine::Engine engine;
    engine.use<engine::PlatformModule>();
    engine.use<engine::RenderModule>();
    engine.use<engine::UiModule>();
    engine.use<engine::VfxModule>();
    engine.use<engine::AudioModule>();
    engine.use<engine::PhysicsModule>();
    engine.use<engine::sandbox::SandboxModule>(smokeTest);

    if (engine.init()) {
        engine.run();
    }
    engine.shutdown();
    return 0;
}
