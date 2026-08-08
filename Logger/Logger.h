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
// File name uses inverted date fields so default A→Z sort
// puts the newest log on top, e.g.:
//   2026-08-05_15-31-29_295 → 0-7973-91-94_84-68-70_704.log
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
