#include "SandboxModule.hpp"
#include "engine/runtime/Engine.hpp"
#include <string>

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") {
            smokeTest = true;
        }
    }

    engine::Engine engine;
    auto& sandbox = engine.use<engine::sandbox::SandboxModule>(smokeTest);
    (void)sandbox;

    if (engine.init()) {
        engine.run();
    }
    engine.shutdown();
    return 0;
}
