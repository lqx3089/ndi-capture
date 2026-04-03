#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>

// Simple thread-safe file logger that writes to logs/<filename>.log
// alongside the executable.
class Logger
{
public:
    static Logger& instance();

    // Opens (or creates) logs/<name>.log next to the executable.
    void open(const std::string& name = "ndi-capture");
    void close();

    void info (const std::string& msg);
    void warn (const std::string& msg);
    void error(const std::string& msg);

private:
    Logger() = default;
    ~Logger();

    void write(const char* level, const std::string& msg);

    std::ofstream  m_file;
    std::mutex     m_mutex;
};

#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
