#pragma once

#include <cstdint>
#include <string_view>

namespace engine {

enum class CmdStatus : uint8_t {
    Success = 0,
    InvalidPosition,
    TileOccupied,
    CannotAfford,
    RequirementsNotMet,
    TargetNotFound,
    ProtectedEntity,
    MaxEraReached,
    UnknownCommand,
    Failure
};

struct CmdResult {
    CmdStatus status = CmdStatus::Success;
    std::string_view message{};

    constexpr bool ok() const { return status == CmdStatus::Success; }
    constexpr explicit operator bool() const { return ok(); }

    static constexpr CmdResult success() {
        return {CmdStatus::Success, {}};
    }

    static constexpr CmdResult fail(CmdStatus s, std::string_view msg = {}) {
        return {s, msg};
    }
};

} // namespace engine
