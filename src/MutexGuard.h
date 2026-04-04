#pragma once
// MutexGuard – RAII wrapper around Win32 named global mutexes.
// Used for:
//   • per-device mutex  : Global\NDICaptureLock_<hash>
//   • per-NDI-name mutex: Global\NDISenderNameLock_<hash>
//
// Usage:
//   MutexGuard g;
//   if (!g.acquire("Global\\NDICaptureLock_abc123")) {
//       // already held by another process
//   }
//   // g released in destructor

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <cstdint>
#include <string>

class MutexGuard
{
public:
    MutexGuard()  = default;
    ~MutexGuard() { release(); }

    // Non-copyable
    MutexGuard(const MutexGuard&)            = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    // Movable
    MutexGuard(MutexGuard&& o) noexcept;
    MutexGuard& operator=(MutexGuard&& o) noexcept;

    // Try to create/acquire a named mutex.
    // Returns true  => we own the mutex.
    // Returns false => another process already owns it (ERROR_ALREADY_EXISTS
    //                  *and* wait-for-ownership not granted).
    bool acquire(const std::string& mutexName);

    void release();

    bool isHeld() const { return m_held; }

    // ── Helpers ──────────────────────────────────────────────────────────
    // Build mutex names from raw strings using a short FNV-1a hex hash.
    static std::string deviceMutexName (const std::string& devicePath);
    static std::string senderMutexName (const std::string& senderName);

private:
#ifdef _WIN32
    HANDLE m_handle = nullptr;
#endif
    bool   m_held   = false;
};
