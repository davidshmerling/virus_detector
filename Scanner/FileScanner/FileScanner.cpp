#include "Scanner/FileScanner/FileScanner.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

FileScanner::FileScanner(
    const AhoCorasick& automaton,
    std::size_t chunk_size)
    : automaton_(automaton),
      chunk_size_(chunk_size == 0 ? 4 * 1024 * 1024 : chunk_size)
{
}

bool FileScanner::scanBuffer(
    const char* data,
    std::size_t size,
    int& state,
    std::size_t& matched_signature_index) const
{
    for (std::size_t i = 0; i < size; ++i) {
        const auto byte = static_cast<unsigned char>(data[i]);
        state = automaton_.nextState(state, byte);

        if (automaton_.hasMatch(state)) {
            matched_signature_index = automaton_.matches(state).front();
            return true;
        }
    }

    return false;
}

FileScanResult FileScanner::scan(const fs::path& file_path) const
{
    FileScanResult result;
    result.file_path = file_path;

    std::error_code error;

    if (!fs::exists(file_path, error)) {
        result.verdict = FileVerdict::Error;

        if (error) {
            result.error = {
                ErrorCode::PermissionDenied,
                "Could not inspect path: " + file_path.string() +
                    " - " + error.message()};
        } else {
            result.error = {
                ErrorCode::PathNotFound,
                "Path does not exist: " + file_path.string()};
        }

        return result;
    }

    if (error) {
        result.verdict = FileVerdict::Error;
        result.error = {
            ErrorCode::PermissionDenied,
            "Could not inspect path: " + file_path.string() +
                " - " + error.message()};
        return result;
    }

    error.clear();

    if (!fs::is_regular_file(file_path, error) || error) {
        result.verdict = FileVerdict::Error;

        if (error) {
            result.error = {
                ErrorCode::PermissionDenied,
                "Could not inspect path: " + file_path.string() +
                    " - " + error.message()};
        } else {
            result.error = {
                ErrorCode::NotRegularFile,
                "Path is not a regular file: " + file_path.string()};
        }

        return result;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        result.verdict = FileVerdict::Error;
        result.error = {
            ErrorCode::FileOpenFailed,
            "Could not open file: " + file_path.string() +
                " - " + std::strerror(errno)};
        return result;
    }

    std::vector<char> buffer(chunk_size_);

    // Important: state lives across chunks, so a signature
    // that crosses a chunk boundary is still detected.
    int state = 0;

    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0) {
            break;
        }

        if (scanBuffer(
                buffer.data(),
                static_cast<std::size_t>(bytes_read),
                state,
                result.matched_signature_index)) {
            result.verdict = FileVerdict::Malicious;
            return result;
        }
    }

    if (file.bad()) {
        result.verdict = FileVerdict::Error;
        result.error = {
            ErrorCode::FileReadFailed,
            "Read error while scanning file: " + file_path.string()};
        return result;
    }

    result.verdict = FileVerdict::Clean;
    return result;
}
