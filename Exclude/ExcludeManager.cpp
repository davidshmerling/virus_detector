#include "Exclude/ExcludeManager.h"

#include <fstream>
#include <string_view>

namespace fs = std::filesystem;

namespace {

const std::unordered_set<std::string> kSystemExcluded = {
    "/proc",
    "/sys",
    "/dev",
    "/run",
    "/tmp",
};

[[nodiscard]] bool isUnderExcludedPrefix(
    std::string_view path,
    std::string_view prefix)
{
    if (path == prefix) {
        return true;
    }

    return path.size() > prefix.size() &&
           path.starts_with(prefix) &&
           path[prefix.size()] == '/';
}

[[nodiscard]] bool isSystemExcluded(std::string_view normalized_path)
{
    for (const std::string& prefix : kSystemExcluded) {
        if (isUnderExcludedPrefix(normalized_path, prefix)) {
            return true;
        }
    }

    return false;
}

}  // namespace

ExcludeManager::ExcludeManager(std::string file_path)
    : file_path_(std::move(file_path))
{
}

std::string ExcludeManager::trim(const std::string& text)
{
    const std::string_view view = text;
    const std::size_t start = view.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }

    const std::size_t end = view.find_last_not_of(" \t\r\n");
    return std::string{view.substr(start, end - start + 1)};
}

fs::path ExcludeManager::normalize(const fs::path& path)
{
    std::error_code error;
    fs::path normalized = fs::weakly_canonical(path, error);

    if (error) {
        normalized = path.lexically_normal();
    }

    return normalized;
}

OperationResult ExcludeManager::load()
{
    excluded_paths_.clear();
    excluded_lookup_.clear();

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        return OperationResult::fail(
            ErrorCode::FileOpenFailed,
            "Could not open exclude file: " + file_path_.string());
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line.starts_with('#')) {
            continue;
        }

        fs::path path(line);

        // For now: only absolute paths are supported.
        if (!path.is_absolute()) {
            continue;
        }

        path = normalize(path);

        const std::string key = path.generic_string();
        if (excluded_lookup_.insert(key).second) {
            excluded_paths_.push_back(std::move(path));
        }
    }

    return OperationResult::ok();
}

bool ExcludeManager::isExcluded(const fs::path& path) const
{
    const fs::path current = normalize(path);
    const std::string normalized_path = current.generic_string();

    if (isSystemExcluded(normalized_path)) {
        return true;
    }

    if (excluded_lookup_.empty()) {
        return false;
    }

    fs::path walk = current;

    while (true) {
        if (excluded_lookup_.contains(walk.generic_string())) {
            return true;
        }

        if (!walk.has_parent_path()) {
            break;
        }

        const fs::path parent = walk.parent_path();
        if (parent == walk) {
            break;
        }

        walk = parent;
    }

    return false;
}

const std::vector<fs::path>& ExcludeManager::getExcludedPaths() const
{
    return excluded_paths_;
}
