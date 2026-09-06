#include "engine/core/Path.hpp"

#include <system_error>

namespace engine {

std::filesystem::path resolvePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return path;
    }

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return path;
    }

#ifdef ENGINE_ROOT_DIR
    const auto fromRoot = std::filesystem::path(ENGINE_ROOT_DIR) / path;
    if (std::filesystem::exists(fromRoot, ec)) {
        return fromRoot;
    }
#endif

#ifdef ENGINE_ASSET_DIR
    const auto fromAssetDir = std::filesystem::path(ENGINE_ASSET_DIR) / path.filename();
    if (std::filesystem::exists(fromAssetDir, ec)) {
        return fromAssetDir;
    }
#endif

    // Search up parent directories from current working directory
    std::filesystem::path cur = std::filesystem::current_path(ec);
    if (!ec) {
        for (int i = 0; i < 6; ++i) {
            const auto candidate = cur / path;
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
            const auto candidateAssets = cur / "assets" / path.filename();
            if (std::filesystem::exists(candidateAssets, ec)) {
                return candidateAssets;
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) {
                break;
            }
            cur = cur.parent_path();
        }
    }

    return path;
}

} // namespace engine

