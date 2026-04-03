#include "MutexGuard.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>
#include <utility>

// ── FNV-1a 32-bit hash ────────────────────────────────────────────────────
static std::string fnv1aHex(const std::string& s)
{
    uint32_t hash = 0x811c9dc5u;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 0x01000193u;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << hash;
    return oss.str();
}

// ── Move ──────────────────────────────────────────────────────────────────
MutexGuard::MutexGuard(MutexGuard&& o) noexcept
{
#ifdef _WIN32
    m_handle   = o.m_handle;
    o.m_handle = nullptr;
#endif
    m_held   = o.m_held;
    o.m_held = false;
}

MutexGuard& MutexGuard::operator=(MutexGuard&& o) noexcept
{
    if (this != &o) {
        release();
#ifdef _WIN32
        m_handle   = o.m_handle;
        o.m_handle = nullptr;
#endif
        m_held   = o.m_held;
        o.m_held = false;
    }
    return *this;
}

// ── acquire / release ─────────────────────────────────────────────────────
bool MutexGuard::acquire(const std::string& mutexName)
{
#ifdef _WIN32
    // Create or open the mutex
    m_handle = CreateMutexA(nullptr, FALSE, mutexName.c_str());
    if (!m_handle) {
        LOG_ERROR("CreateMutex failed for: " + mutexName);
        return false;
    }

    // Try to take ownership without waiting
    DWORD waitResult = WaitForSingleObject(m_handle, 0);
    if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
        m_held = true;
        LOG_INFO("Acquired mutex: " + mutexName);
        return true;
    }

    // Mutex is held by someone else
    CloseHandle(m_handle);
    m_handle = nullptr;
    LOG_WARN("Mutex already held: " + mutexName);
    return false;
#else
    // Non-Windows stub – always succeeds (single-platform target)
    m_held = true;
    return true;
#endif
}

void MutexGuard::release()
{
#ifdef _WIN32
    if (m_held && m_handle) {
        ReleaseMutex(m_handle);
        m_held = false;
    }
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
#else
    m_held = false;
#endif
}

// ── Name helpers ──────────────────────────────────────────────────────────
std::string MutexGuard::deviceMutexName(const std::string& devicePath)
{
    return "Global\\NDICaptureLock_" + fnv1aHex(devicePath);
}

std::string MutexGuard::senderMutexName(const std::string& senderName)
{
    return "Global\\NDISenderNameLock_" + fnv1aHex(senderName);
}
