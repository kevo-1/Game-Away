#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace GameAway {

class Crypto {
public:
    // Initialize crypto with a token (derives encryption key)
    explicit Crypto(const std::string& token);
    ~Crypto();
    
    // Encrypt plaintext, returns base64-encoded ciphertext with nonce
    std::string encrypt(const std::string& plaintext);
    
    // Decrypt base64-encoded ciphertext, returns plaintext
    std::string decrypt(const std::string& ciphertext);
    
    // Check if crypto is properly initialized
    bool isValid() const { return m_valid; }

private:
    std::vector<uint8_t> m_key;
    bool m_valid = false;
    void* m_hAlg = nullptr;
    
    // Derive key from token using PBKDF2
    bool deriveKey(const std::string& token);
};

// Base64 encoding/decoding utilities
std::string base64Encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64Decode(const std::string& encoded);

} // namespace GameAway
