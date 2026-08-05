#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel {
    Info,
    Warning,
    Error
};

// Creates one new log file per run.
// File name example: runtime/logs/2026-08-05_09-23-45_123.log
//
// Sorted by name descending (Z→A) or by date => newest file on top.
class Logger {
public:
    // Creates logs_directory if needed, then opens a new timestamped file.
    explicit Logger(const std::string& logs_directory = "logs");

    bool isOpen() const;
    const std::string& filePath() const;

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    void write(LogLevel level, const std::string& message);

    static std::string levelToString(LogLevel level);
    static std::string currentTimeText();
    static std::string makeFileName();

    std::string file_path_;
    std::ofstream file_;
    std::mutex mutex_;
};
