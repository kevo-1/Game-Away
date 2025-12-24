#include "token.hpp"
#include "config.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <random>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")

namespace GameAway {

std::string generateToken(size_t length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const size_t charsetSize = sizeof(charset) - 1;
    
    std::string token;
    token.reserve(length);
    
    // Use BCryptGenRandom for cryptographically secure random bytes
    std::vector<BYTE> randomBytes(length);
    NTSTATUS status = BCryptGenRandom(
        nullptr,
        randomBytes.data(),
        static_cast<ULONG>(length),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    
    if (status != 0) {
        // Fallback to std::random_device if BCrypt fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(charsetSize - 1));
        for (size_t i = 0; i < length; ++i) {
            token += charset[dis(gen)];
        }
        return token;
    }
    
    for (size_t i = 0; i < length; ++i) {
        token += charset[randomBytes[i] % charsetSize];
    }
    
    return token;
}

std::string getPcName() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    
    return "Unknown";
}

} // namespace GameAway
