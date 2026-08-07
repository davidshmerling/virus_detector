#include "Logger/Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

Logger::Logger(const std::string& logs_directory)
{
    std::error_code error;
    fs::create_directories(logs_directory, error);

    file_path_ = (fs::path(logs_directory) / makeFileName()).string();

    // New file for this run (not append to an old one).
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
        case LogLevel::Info:
            return "INFO";

        case LogLevel::Warning:
            return "WARNING";

        case LogLevel::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

std::string Logger::currentTimeText()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

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

    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    // Invert calendar fields so default A→Z name sort puts newest first.
    // Leading "0-" keeps new logs above legacy "2026-..." names.
    // Example for 2026-08-05_15-31-29_295 → 0-7973-91-94_84-68-70_704.log
    std::ostringstream stream;
    stream << "0-"
           << std::setfill('0')
           << std::setw(4) << (9999 - (local_time.tm_year + 1900)) << '-'
           << std::setw(2) << (99 - (local_time.tm_mon + 1)) << '-'
           << std::setw(2) << (99 - local_time.tm_mday) << '_'
           << std::setw(2) << (99 - local_time.tm_hour) << '-'
           << std::setw(2) << (99 - local_time.tm_min) << '-'
           << std::setw(2) << (99 - local_time.tm_sec) << '_'
           << std::setw(3) << (999 - milliseconds.count())
           << ".log";
    return stream.str();
}
