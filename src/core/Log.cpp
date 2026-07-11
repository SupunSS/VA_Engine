#include "Log.h"
#include <iostream>
#include <chrono>

void Log::Write(LogLevel level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::current_zone()->to_local(now);

    const char* levelStr = "INFO";
    if (level == LogLevel::Warn)  levelStr = "WARN";
    if (level == LogLevel::Error) levelStr = "ERROR";

    std::cout << std::format("[{:%H:%M:%S}] [{}] {}\n", time, levelStr, message);
}