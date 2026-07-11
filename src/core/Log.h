#pragma once
#include <string>
#include <format>

enum class LogLevel { Info, Warn, Error };

class Log {
public:
    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args) {
        Write(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warn(std::format_string<Args...> fmt, Args&&... args) {
        Write(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args) {
        Write(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static void Write(LogLevel level, const std::string& message);
};