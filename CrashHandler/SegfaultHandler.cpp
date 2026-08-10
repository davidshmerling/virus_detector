#include "CrashHandler/SegfaultHandler.h"

#include <csignal>
#include <execinfo.h>
#include <unistd.h>

namespace {

void onSegmentationFault(int signal)
{
    constexpr char message[] =
        "\nFatal error: segmentation fault\nStack trace:\n";

    const ssize_t written =
        ::write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)written;

    void* frames[64];
    const int frameCount = ::backtrace(frames, 64);

    ::backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);

    _exit(128 + signal);
}

}  // namespace

void installSegfaultHandler()
{
    // Reports the fault with a backtrace on STDERR, then exits immediately.
    std::signal(SIGSEGV, onSegmentationFault);
}
