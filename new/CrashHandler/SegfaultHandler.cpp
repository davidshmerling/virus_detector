#include "CrashHandler/SegfaultHandler.h"

#include <csignal>
#include <unistd.h>

namespace {

// Signal handlers may only call async-signal-safe functions.
void writeMessage(const char* message)
{
    if (message == nullptr) {
        return;
    }

    std::size_t length = 0;
    while (message[length] != '\0') {
        ++length;
    }

    const ssize_t written = ::write(STDERR_FILENO, message, length);
    (void)written;
}

void onSegmentationFault(int /*signal*/)
{
    writeMessage("Fatal error: segmentation fault\n");
    _exit(1);
}

} // namespace

void installSegfaultHandler()
{
    std::signal(SIGSEGV, onSegmentationFault);
}
