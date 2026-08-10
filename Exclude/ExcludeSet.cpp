#include "Exclude/ExcludeSet.h"

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Paths the product must never scan: OS virtual/temp trees, plus the scanner's
// own development and installed layouts. User excludes from exclude.txt are
// layered on top of these.
const std::vector<fs::path> kDefaultExcluded = {
    "/proc",
    "/sys",
    "/dev",
    "/run",
    "/tmp",
    "/workspace/virus_detector",
    "/opt/virus-detector",
};

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

}  // namespace

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

bool ExcludeSet::load()
{
    excluded_paths_.clear();

    for (const fs::path& path : kDefaultExcluded) {
        excluded_paths_.insert(normalize(path));
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
