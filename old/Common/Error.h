#pragma once

#include <format>
#include <string>

enum class ErrorCode {
    None,
    InvalidArgument,
    FileOpenFailed,
    FileReadFailed,
    QuarantineFailed,
    CheckpointFailed,
    Unknown
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
};

[[nodiscard]] inline const char* errorCodeToString(ErrorCode code) noexcept
{
    switch (code) {
        case ErrorCode::None:
            return "None";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::FileOpenFailed:
            return "FileOpenFailed";
        case ErrorCode::FileReadFailed:
            return "FileReadFailed";
        case ErrorCode::QuarantineFailed:
            return "QuarantineFailed";
        case ErrorCode::CheckpointFailed:
            return "CheckpointFailed";
        case ErrorCode::Unknown:
        default:
            return "Unknown";
    }
}

[[nodiscard]] inline std::string formatError(const Error& error)
{
    return std::format(
        "[{}] {}",
        errorCodeToString(error.code),
        error.message);
}
