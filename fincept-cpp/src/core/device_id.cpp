#include "device_id.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cstdlib>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fincept::utils {

std::string generate_device_id() {
    // Gather system info similar to TypeScript's navigator properties
    std::ostringstream info;

#ifdef _WIN32
    // Platform
    info << "Win32;";

    // Screen resolution
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    info << w << "x" << h << ";";

    // Computer name
    char computer_name[256] = {};
    DWORD size = sizeof(computer_name);
    GetComputerNameA(computer_name, &size);
    info << computer_name << ";";

    // Processor count
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info << si.dwNumberOfProcessors << ";";
#else
    info << "Desktop;";
    const char* user = std::getenv("USER");
    if (user) info << user << ";";
    const char* home = std::getenv("HOME");
    if (home) info << home << ";";
#endif

    // Hash the info string
    std::string info_str = info.str();
    std::hash<std::string> hasher;
    size_t hash = hasher(info_str);

    // Timestamp component (base36)
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    // Random component
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t random_val = dist(gen);

    // Format: fincept_desktop_{hash}_{timestamp}_{random}
    std::ostringstream device_id;
    device_id << "fincept_desktop_"
              << std::hex << hash << "_"
              << std::hex << ms << "_"
              << std::hex << random_val;

    std::string result = device_id.str();

    // Ensure 10-255 characters
    if (result.size() > 255) result.resize(255);
    return result;
}

} // namespace fincept::utils
