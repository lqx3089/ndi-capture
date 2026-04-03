#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

Logger& Logger::instance()
{
    static Logger s_inst;
    return s_inst;
}

Logger::~Logger()
{
    close();
}

void Logger::open(const std::string& name)
{
    std::lock_guard<std::mutex> lk(m_mutex);

    // Determine exe directory (Windows) or cwd (fallback)
    std::filesystem::path logDir;
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    logDir = std::filesystem::path(buf).parent_path() / "logs";
#else
    logDir = std::filesystem::current_path() / "logs";
#endif
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);

    auto logFile = logDir / (name + ".log");
    m_file.open(logFile, std::ios::app);
    if (!m_file.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << logFile << "\n";
    }
}

void Logger::close()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_file.is_open()) m_file.close();
}

void Logger::write(const char* level, const std::string& msg)
{
    // ISO-8601-ish timestamp
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << " [" << level << "] " << msg << "\n";
    const auto line = oss.str();

    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_file.is_open()) {
        m_file << line;
        m_file.flush();
    }
    std::cerr << line;
}

void Logger::info (const std::string& msg) { write("INFO ", msg); }
void Logger::warn (const std::string& msg) { write("WARN ", msg); }
void Logger::error(const std::string& msg) { write("ERROR", msg); }
