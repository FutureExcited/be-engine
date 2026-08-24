#include "BePlatform.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

auto BePlatform::GetExecutableDirectory() -> std::filesystem::path {
#if defined(_WIN32)
    char buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (n == 0 || n == MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::string(buffer, n)).parent_path();
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::weakly_canonical(buffer).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(__linux__)
    char buffer[4096];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer));
    if (n > 0) {
        return std::filesystem::path(std::string(buffer, size_t(n))).parent_path();
    }
    return std::filesystem::current_path();
#else
    return std::filesystem::current_path();
#endif
}

auto BePlatform::MoveWorkingDirectoryToExecutableDir() -> void {
    std::filesystem::current_path(GetExecutableDirectory());
}
