#include "EraSandboxModule.hpp"
#include "engine/runtime/Engine.hpp"

int main(int argc, char** argv) {
    engine::Engine engine;
    engine.modules().registerModule<engine::era::EraSandboxModule>();
    return engine.run();
}
