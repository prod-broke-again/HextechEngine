#include "EraSandboxModule.hpp"
#include "engine/runtime/Engine.hpp"
#include "engine/runtime/modules/PlatformModule.hpp"
#include "engine/runtime/modules/RenderModule.hpp"
#include "engine/runtime/modules/UiModule.hpp"
#include "engine/runtime/modules/VfxModule.hpp"
#include "engine/runtime/modules/AudioModule.hpp"
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
    engine.use<engine::era::EraSandboxModule>();

    if (engine.init()) {
        engine.run();
    }
    engine.shutdown();
    return 0;
}
