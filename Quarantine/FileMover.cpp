#include "Quarantine/FileMover.h"

#include <system_error>

namespace fs = std::filesystem;

bool FileMover::move(
    const fs::path& source,
    const fs::path& destination) const
{
    std::error_code error;
    fs::rename(source, destination, error);

    if (!error) {
        return true;
    }

    // Never overwrite an existing destination.
    fs::copy_file(
        source,
        destination,
        fs::copy_options::none,
        error);

    if (error) {
        return false;
    }

    fs::remove(source, error);
    return !error;
}
