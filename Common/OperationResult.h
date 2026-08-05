#pragma once

#include "Common/Error.h"

#include <string>
#include <utility>

struct [[nodiscard]] OperationResult {
    bool success = false;
    Error error{};

    explicit operator bool() const noexcept
    {
        return success;
    }

    static OperationResult ok()
    {
        return OperationResult{.success = true};
    }

    static OperationResult fail(
        ErrorCode code,
        std::string message)
    {
        return OperationResult{
            .success = false,
            .error = Error{
                .code = code,
                .message = std::move(message)}};
    }
};
