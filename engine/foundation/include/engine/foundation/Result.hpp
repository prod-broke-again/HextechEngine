#pragma once

#include <string>
#include <variant>
#include <utility>
#include <stdexcept>
#include <string_view>

namespace engine {

// A simple result type combining a value (or void) and an error code/message
template <typename T, typename E>
class Result {
public:
    // Success constructor
    Result(T value) : m_data(std::move(value)) {}
    
    // Error constructor
    Result(E error) : m_data(std::move(error)) {}

    bool isOk() const { return std::holds_alternative<T>(m_data); }
    bool isErr() const { return std::holds_alternative<E>(m_data); }

    T& value() {
        if (!isOk()) throw std::runtime_error("Result is error");
        return std::get<T>(m_data);
    }

    const T& value() const {
        if (!isOk()) throw std::runtime_error("Result is error");
        return std::get<T>(m_data);
    }

    E& error() {
        if (!isErr()) throw std::runtime_error("Result is ok");
        return std::get<E>(m_data);
    }

    const E& error() const {
        if (!isErr()) throw std::runtime_error("Result is ok");
        return std::get<E>(m_data);
    }

private:
    std::variant<T, E> m_data;
};

// Specialization for void
template <typename E>
class Result<void, E> {
public:
    Result() : m_isOk(true) {}
    Result(E error) : m_isOk(false), m_error(std::move(error)) {}

    bool isOk() const { return m_isOk; }
    bool isErr() const { return !m_isOk; }

    E& error() {
        if (!isErr()) throw std::runtime_error("Result is ok");
        return m_error;
    }

    const E& error() const {
        if (!isErr()) throw std::runtime_error("Result is ok");
        return m_error;
    }

private:
    bool m_isOk;
    E m_error{};
};

enum class CmdErrorCode {
    Success = 0,
    InvalidParameters,
    NotEnoughResources,
    EntityNotFound,
    InvalidState,
    NotAllowed
};

// Helper for commands
using CmdResult = Result<void, CmdErrorCode>;

// Helpers for returning
inline CmdResult CmdSuccess() {
    return CmdResult();
}

inline CmdResult CmdError(CmdErrorCode code) {
    return CmdResult(code);
}

} // namespace engine
