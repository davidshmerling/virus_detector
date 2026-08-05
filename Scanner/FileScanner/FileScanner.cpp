#include "Scanner/FileScanner/FileScanner.h"

#include <cerrno>
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
    std::span<const char> data,
    int& state,
    std::size_t& matched_signature_index) const
{
    for (const char byte_value : data) {
        const auto byte = static_cast<unsigned char>(byte_value);
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
    FileScanResult result{
        .verdict = FileVerdict::Error,
        .file_path = file_path};

    std::error_code error;

    if (!fs::exists(file_path, error)) {
        if (error) {
            result.error = {
                .code = ErrorCode::PermissionDenied,
                .message = "Could not inspect path: " + file_path.string() +
                           " - " + error.message()};
        } else {
            result.error = {
                .code = ErrorCode::PathNotFound,
                .message = "Path does not exist: " + file_path.string()};
        }

        return result;
    }

    if (error) {
        result.error = {
            .code = ErrorCode::PermissionDenied,
            .message = "Could not inspect path: " + file_path.string() +
                       " - " + error.message()};
        return result;
    }

    error.clear();

    if (!fs::is_regular_file(file_path, error) || error) {
        if (error) {
            result.error = {
                .code = ErrorCode::PermissionDenied,
                .message = "Could not inspect path: " + file_path.string() +
                           " - " + error.message()};
        } else {
            result.error = {
                .code = ErrorCode::NotRegularFile,
                .message =
                    "Path is not a regular file: " + file_path.string()};
        }

        return result;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        result.error = {
            .code = ErrorCode::FileOpenFailed,
            .message = "Could not open file: " + file_path.string() +
                       " - " +
                       std::system_category().message(errno)};
        return result;
    }

    std::vector<char> buffer(chunk_size_);

    // Important: state lives across chunks, so a signature
    // that crosses a chunk boundary is still detected.
    int state = 0;

    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytes_read = file.gcount();

        if (bytes_read <= 0) {
            break;
        }

        if (scanBuffer(
                std::span<const char>{
                    buffer.data(),
                    static_cast<std::size_t>(bytes_read)},
                state,
                result.matched_signature_index)) {
            result.verdict = FileVerdict::Malicious;
            return result;
        }
    }

    if (file.bad()) {
        result.error = {
            .code = ErrorCode::FileReadFailed,
            .message =
                "Read error while scanning file: " + file_path.string()};
        return result;
    }

    result.verdict = FileVerdict::Clean;
    return result;
}
