#pragma once

#include <fstream>
#include <mutex>
#include <string>

// Severity tag written into each log line.
enum class LogLevel {
    Info,
    Warning,
    Error
};

// Creates one new log file per run with a readable timestamp name, for example:
//   2026-08-09_21-49-00_123.log
// Timestamps use a fixed Israel wall clock (UTC+3), independent of container TZ.
class Logger {
public:
    // Creates `logs_directory` if needed, then opens a new timestamped file.
    explicit Logger(const std::string& logs_directory = "logs");

    bool isOpen() const;
    const std::string& filePath() const;

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    // Writes one leveled line to the log file under the mutex.
    void write(LogLevel level, const std::string& message);

    static std::string levelToString(LogLevel level);
    // Formats "YYYY-MM-DD HH:MM:SS" in Israel local time.
    static std::string currentTimeText();
    // Builds the per-run filename including milliseconds.
    static std::string makeFileName();

    std::string file_path_;
    std::ofstream file_;
    std::mutex mutex_;
};
