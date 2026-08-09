#include "Exclude/ExcludeManager.h"

#include <fstream>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

const std::unordered_set<std::string> kSystemExcluded = {
    "/proc",
    "/sys",
    "/dev",
    "/run",
    "/tmp",
};

// Walks up from the working directory to the project root (the directory
// holding .git), falling back to the working directory if no repository marker
// is found.
fs::path findProjectRoot()
{
    std::error_code error;
    const fs::path base = fs::current_path(error);
    if (error) {
        return {};
    }

    for (fs::path dir = base; !dir.empty(); dir = dir.parent_path()) {
        std::error_code exists_error;
        if (fs::exists(dir / ".git", exists_error) && !exists_error) {
            return dir;
        }
        if (dir == dir.root_path()) {
            break;
        }
    }

    return base;
}

}  // namespace

ExcludeManager::ExcludeManager(fs::path file_path)
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

bool ExcludeManager::isUnderPrefix(
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

void ExcludeManager::addExact(const fs::path& path, bool internal)
{
    const std::string key = normalize(path).generic_string();
    exact_.insert(key);
    if (internal) {
        internal_.insert(key);
    }
}

bool ExcludeManager::load()
{
    exact_.clear();
    internal_.clear();

    for (const std::string& system_path : kSystemExcluded) {
        exact_.insert(system_path);
    }

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.starts_with('#')) {
            continue;
        }

        fs::path path(line);
        if (!path.is_absolute()) {
            continue;
        }

        addExact(path, false);
    }

    return true;
}

void ExcludeManager::excludeProjectRoot()
{
    const fs::path root = findProjectRoot();
    if (root.empty()) {
        return;
    }

    clearInternalExcludedPaths();
    addInternalExcludedPath(root);
}

void ExcludeManager::clearInternalExcludedPaths()
{
    for (const std::string& key : internal_) {
        exact_.erase(key);
    }
    internal_.clear();
}

void ExcludeManager::addInternalExcludedPath(const fs::path& path)
{
    addExact(path, true);
}

bool ExcludeManager::contains(const fs::path& path) const
{
    return exact_.contains(normalize(path).generic_string());
}

bool ExcludeManager::shouldSkip(const fs::path& path) const
{
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error) {
        return true;
    }
    if (status.type() == fs::file_type::symlink) {
        return true;
    }

    // Exact exclude only — parents already passed DFS exclusion.
    return contains(path);
}

bool ExcludeManager::isScanRootExcluded(const fs::path& root) const
{
    const std::string key = normalize(root).generic_string();

    for (const std::string& prefix : exact_) {
        if (isUnderPrefix(key, prefix)) {
            return true;
        }
    }

    return false;
}
