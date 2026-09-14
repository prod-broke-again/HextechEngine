#include "EraSandboxModule.hpp"
#include "engine/runtime/Engine.hpp"

int main(int argc, char** argv) {
    engine::Engine engine;
    engine.use<engine::era::EraSandboxModule>();
    if (engine.init()) {
        engine.run();
    }
    engine.shutdown();
    return 0;
}
