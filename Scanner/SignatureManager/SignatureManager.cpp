#include "Scanner/SignatureManager/SignatureManager.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace fs = std::filesystem;

SignatureManager::SignatureManager(std::string file_path)
    : file_path_(std::move(file_path))
{
}

std::string SignatureManager::trim(const std::string& text)
{
    const std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

OperationResult SignatureManager::load()
{
    signatures_.clear();
    max_signature_length_ = 0;

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        return OperationResult::fail(
            ErrorCode::FileOpenFailed,
            "Could not open signatures file: " + file_path_);
    }

    std::unordered_set<std::string> seen;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (seen.find(line) != seen.end()) {
            continue;
        }

        seen.insert(line);
        signatures_.push_back(line);

        if (line.size() > max_signature_length_) {
            max_signature_length_ = line.size();
        }
    }

    if (signatures_.empty()) {
        return OperationResult::fail(
            ErrorCode::InvalidArgument,
            "No valid signatures loaded from: " + file_path_);
    }

    std::error_code error;
    const fs::file_time_type file_time =
        fs::last_write_time(file_path_, error);

    if (error) {
        last_modified_ = 0;
    } else {
        last_modified_ = static_cast<std::int64_t>(
            file_time.time_since_epoch().count());
    }

    return OperationResult::ok();
}

const std::vector<std::string>& SignatureManager::getSignatures() const
{
    return signatures_;
}

std::size_t SignatureManager::count() const
{
    return signatures_.size();
}

std::size_t SignatureManager::maxSignatureLength() const
{
    return max_signature_length_;
}

std::int64_t SignatureManager::lastModified() const
{
    return last_modified_;
}
