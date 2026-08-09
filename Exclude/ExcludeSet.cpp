#include "Exclude/ExcludeSet.h"

#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// --- constants ---------------------------------------------------------------

const std::unordered_set<std::string> kSystemExcluded = {
    "/proc",
    "/sys",
    "/dev",
    "/run",
    "/tmp",
};

// --- path helpers ------------------------------------------------------------

std::string trim(const std::string& text)
{
    const std::string_view view = text;
    const std::size_t start = view.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = view.find_last_not_of(" \t\r\n");
    return std::string{view.substr(start, end - start + 1)};
}

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

// --- construction ------------------------------------------------------------

ExcludeSet::ExcludeSet(fs::path file_path)
    : file_path_(std::move(file_path))
{
}

std::string ExcludeSet::normalize(const fs::path& path)
{
    std::error_code error;
    fs::path normalized = fs::weakly_canonical(path, error);
    if (error) {
        normalized = path.lexically_normal();
    }
    return normalized.generic_string();
}

// --- loading -----------------------------------------------------------------

bool ExcludeSet::load()
{
    excluded_paths_.clear();

    for (const std::string& system_path : kSystemExcluded) {
        excluded_paths_.insert(system_path);
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

        const fs::path path(line);
        if (path.is_absolute()) {
            excluded_paths_.insert(normalize(path));
        }
    }

    return true;
}

void ExcludeSet::excludeProjectRoot()
{
    const fs::path root = findProjectRoot();
    if (root.empty()) {
        return;
    }

    excluded_paths_.insert(normalize(root));
}
