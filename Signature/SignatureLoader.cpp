#include "Signature/SignatureLoader.h"

#include <fstream>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

SignatureLoader::SignatureLoader(fs::path file_path)
    : file_path_(std::move(file_path))
{
}

std::string SignatureLoader::trim(const std::string& text)
{
    const std::string_view view = text;
    const std::size_t start = view.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = view.find_last_not_of(" \t\r\n");
    return std::string{view.substr(start, end - start + 1)};
}

bool SignatureLoader::isValidSignature(const std::string& line)
{
    return !line.empty() && !line.starts_with('#');
}

bool SignatureLoader::load()
{
    signatures_.clear();
    last_modified_ = 0;

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        if (!isValidSignature(line)) {
            continue;
        }

        signatures_.push_back(std::move(line));
    }

    std::error_code error;
    const fs::file_time_type write_time = fs::last_write_time(file_path_, error);
    if (!error) {
        last_modified_ =
            static_cast<std::int64_t>(write_time.time_since_epoch().count());
    }

    return !signatures_.empty();
}

const std::vector<std::string>& SignatureLoader::signatures() const
{
    return signatures_;
}

std::size_t SignatureLoader::count() const
{
    return signatures_.size();
}

std::int64_t SignatureLoader::lastModified() const
{
    return last_modified_;
}
