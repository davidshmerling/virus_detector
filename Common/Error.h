#pragma once

#include <format>
#include <string>

enum class ErrorCode {
    None,

    InvalidArgument,
    PathNotFound,
    PermissionDenied,
    NotRegularFile,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed,
    FileMoveFailed,
    FileDeleteFailed,

    InvalidJson,
    RepositoryLoadFailed,
    RepositorySaveFailed,

    CacheFailed,
    QuarantineFailed,
    CheckpointFailed,
    EnumerationFailed,
    ThreadPoolStopped,

    Unknown
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;

    [[nodiscard]] bool hasError() const noexcept
    {
        return code != ErrorCode::None;
    }
};

[[nodiscard]] inline const char* errorCodeToString(ErrorCode code) noexcept
{
    switch (code) {
        case ErrorCode::None:
            return "None";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::PathNotFound:
            return "PathNotFound";
        case ErrorCode::PermissionDenied:
            return "PermissionDenied";
        case ErrorCode::NotRegularFile:
            return "NotRegularFile";
        case ErrorCode::FileOpenFailed:
            return "FileOpenFailed";
        case ErrorCode::FileReadFailed:
            return "FileReadFailed";
        case ErrorCode::FileWriteFailed:
            return "FileWriteFailed";
        case ErrorCode::FileMoveFailed:
            return "FileMoveFailed";
        case ErrorCode::FileDeleteFailed:
            return "FileDeleteFailed";
        case ErrorCode::InvalidJson:
            return "InvalidJson";
        case ErrorCode::RepositoryLoadFailed:
            return "RepositoryLoadFailed";
        case ErrorCode::RepositorySaveFailed:
            return "RepositorySaveFailed";
        case ErrorCode::CacheFailed:
            return "CacheFailed";
        case ErrorCode::QuarantineFailed:
            return "QuarantineFailed";
        case ErrorCode::CheckpointFailed:
            return "CheckpointFailed";
        case ErrorCode::EnumerationFailed:
            return "EnumerationFailed";
        case ErrorCode::ThreadPoolStopped:
            return "ThreadPoolStopped";
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
