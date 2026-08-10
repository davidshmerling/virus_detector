#include "Logger/Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace {

constexpr std::time_t kIsraelOffsetSeconds = 3 * 60 * 60;

// Fixed Israel wall clock (UTC+3), independent of the container TZ.
std::tm israelLocalTime(std::time_t utc)
{
    const std::time_t israel = utc + kIsraelOffsetSeconds;
    std::tm local_time{};
    gmtime_r(&israel, &local_time);
    return local_time;
}

}  // namespace

Logger::Logger(const std::string& logs_directory)
{
    std::error_code error;
    fs::create_directories(logs_directory, error);

    file_path_ = (fs::path(logs_directory) / makeFileName()).string();

    // Open a new file for this run; do not append to an old one.
    file_.open(file_path_, std::ios::out | std::ios::trunc);
}

bool Logger::isOpen() const
{
    return file_.is_open();
}

const std::string& Logger::filePath() const
{
    return file_path_;
}

void Logger::info(const std::string& message)
{
    write(LogLevel::Info, message);
}

void Logger::warning(const std::string& message)
{
    write(LogLevel::Warning, message);
}

void Logger::error(const std::string& message)
{
    write(LogLevel::Error, message);
}

void Logger::write(LogLevel level, const std::string& message)
{
    std::scoped_lock lock(mutex_);

    if (!file_.is_open()) {
        return;
    }

    file_ << '[' << currentTimeText() << "] "
          << '[' << levelToString(level) << "] "
          << message << '\n';

    file_.flush();
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
    }

    return "UNKNOWN";
}

std::string Logger::currentTimeText()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = israelLocalTime(time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string Logger::makeFileName()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

    const std::tm local_time = israelLocalTime(time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S") << '_'
           << std::setfill('0') << std::setw(3) << milliseconds.count()
           << ".log";
    return stream.str();
}
