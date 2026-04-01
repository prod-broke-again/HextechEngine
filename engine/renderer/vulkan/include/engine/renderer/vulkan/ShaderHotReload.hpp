#pragma once

namespace engine {

void shaderHotReloadWatchPath(const char* path);
bool shaderHotReloadPollChanged(const char* path);

} // namespace engine
