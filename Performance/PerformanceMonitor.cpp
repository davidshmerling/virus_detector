#include "Performance/PerformanceMonitor.h"

#include "Console/ConsolePrinter.h"

#include <iomanip>
#include <sstream>
#include <utility>

PerformanceMonitor::PerformanceMonitor(Logger& logger, std::string label)
    : logger_(logger),
      label_(std::move(label)),
      start_(std::chrono::steady_clock::now())
{
}

PerformanceMonitor::~PerformanceMonitor()
{
    stop();
}

void PerformanceMonitor::stop()
{
    if (stopped_) {
        return;
    }
    stopped_ = true;

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);

    const std::string text = label_ + ": " + formatDuration(elapsed);

    logger_.info(text);
    ConsolePrinter::printMessage(text);
}

std::string PerformanceMonitor::formatDuration(std::chrono::nanoseconds elapsed)
{
    const double seconds = std::chrono::duration<double>(elapsed).count();
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << seconds << " s (" << millis
           << " ms)";
    return stream.str();
}
