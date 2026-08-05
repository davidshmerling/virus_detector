#pragma once

#include "Common/Error.h"

#include <string>

struct OperationResult {
    bool success = false;
    Error error;

    static OperationResult ok()
    {
        OperationResult result;
        result.success = true;
        return result;
    }

    static OperationResult fail(
        ErrorCode code,
        const std::string& message)
    {
        OperationResult result;
        result.success = false;
        result.error.code = code;
        result.error.message = message;
        return result;
    }
};
