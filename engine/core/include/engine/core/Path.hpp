#pragma once

#include <filesystem>

namespace engine {

[[nodiscard]] std::filesystem::path resolvePath(const std::filesystem::path& path);

} // namespace engine
