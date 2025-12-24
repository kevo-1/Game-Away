#include "crypto.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <random>

#pragma comment(lib, "bcrypt.lib")

namespace GameAway {

// AES-256 constants
constexpr ULONG KEY_SIZE = 32;  // 256 bits
constexpr ULONG NONCE_SIZE = 12; // 96 bits for GCM
constexpr ULONG TAG_SIZE = 16;   // 128 bits auth tag

// Base64 character set
static const char base64Chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        result += base64Chars[(triple >> 18) & 0x3F];
        result += base64Chars[(triple >> 12) & 0x3F];
        result += base64Chars[(triple >> 6) & 0x3F];
        result += base64Chars[triple & 0x3F];
    }
    
    // Add padding
    size_t mod = data.size() % 3;
    if (mod == 1) {
        result[result.size() - 2] = '=';
        result[result.size() - 1] = '=';
    } else if (mod == 2) {
        result[result.size() - 1] = '=';
    }
    
    return result;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    static const int decodeTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::vector<uint8_t> result;
    result.reserve((encoded.size() / 4) * 3);
    
    int val = 0, valb = -8;
    for (char c : encoded) {
        if (c == '=') break;
        int idx = decodeTable[static_cast<unsigned char>(c)];
        if (idx == -1) continue;
        val = (val << 6) + idx;
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result;
}

Crypto::Crypto(const std::string& token) {
    m_valid = deriveKey(token);
    
    if (m_valid) {
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            reinterpret_cast<BCRYPT_ALG_HANDLE*>(&m_hAlg),
            BCRYPT_AES_ALGORITHM,
            nullptr,
            0
        );
        
        if (status != 0) {
            m_valid = false;
            return;
        }
        
        status = BCryptSetProperty(
            reinterpret_cast<BCRYPT_ALG_HANDLE>(m_hAlg),
            BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
            sizeof(BCRYPT_CHAIN_MODE_GCM),
            0
        );
        
        if (status != 0) {
            BCryptCloseAlgorithmProvider(reinterpret_cast<BCRYPT_ALG_HANDLE>(m_hAlg), 0);
            m_hAlg = nullptr;
            m_valid = false;
        }
    }
}

Crypto::~Crypto() {
    if (m_hAlg) {
        BCryptCloseAlgorithmProvider(reinterpret_cast<BCRYPT_ALG_HANDLE>(m_hAlg), 0);
    }
    // Securely clear the key
    SecureZeroMemory(m_key.data(), m_key.size());
}

bool Crypto::deriveKey(const std::string& token) {
    // Use PBKDF2 to derive a 256-bit key from the token
    BCRYPT_ALG_HANDLE hPrf = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hPrf,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG
    );
    
    if (status != 0) return false;
    
    // Salt derived from application name
    const char* salt = "GameAway_v1.0_Salt";
    
    m_key.resize(KEY_SIZE);
    
    status = BCryptDeriveKeyPBKDF2(
        hPrf,
        reinterpret_cast<PUCHAR>(const_cast<char*>(token.data())),
        static_cast<ULONG>(token.size()),
        reinterpret_cast<PUCHAR>(const_cast<char*>(salt)),
        static_cast<ULONG>(strlen(salt)),
        100000,  // iterations
        m_key.data(),
        KEY_SIZE,
        0
    );
    
    BCryptCloseAlgorithmProvider(hPrf, 0);
    
    return status == 0;
}

std::string Crypto::encrypt(const std::string& plaintext) {
    if (!m_valid) return "";
    
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status = BCryptGenerateSymmetricKey(
        reinterpret_cast<BCRYPT_ALG_HANDLE>(m_hAlg),
        &hKey,
        nullptr,
        0,
        m_key.data(),
        KEY_SIZE,
        0
    );
    
    if (status != 0) return "";
    
    // Generate random nonce
    std::vector<uint8_t> nonce(NONCE_SIZE);
    BCryptGenRandom(nullptr, nonce.data(), NONCE_SIZE, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    // Setup auth info
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = NONCE_SIZE;
    authInfo.pbTag = nullptr;
    authInfo.cbTag = TAG_SIZE;
    
    // Get output size
    ULONG ciphertextSize = 0;
    status = BCryptEncrypt(
        hKey,
        reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
        static_cast<ULONG>(plaintext.size()),
        &authInfo,
        nullptr,
        0,
        nullptr,
        0,
        &ciphertextSize,
        0
    );
    
    if (status != 0) {
        BCryptDestroyKey(hKey);
        return "";
    }
    
    std::vector<uint8_t> tag(TAG_SIZE);
    std::vector<uint8_t> ciphertext(ciphertextSize);
    
    authInfo.pbTag = tag.data();
    
    status = BCryptEncrypt(
        hKey,
        reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
        static_cast<ULONG>(plaintext.size()),
        &authInfo,
        nullptr,
        0,
        ciphertext.data(),
        ciphertextSize,
        &ciphertextSize,
        0
    );
    
    BCryptDestroyKey(hKey);
    
    if (status != 0) return "";
    
    // Combine: nonce + tag + ciphertext
    std::vector<uint8_t> combined;
    combined.reserve(NONCE_SIZE + TAG_SIZE + ciphertextSize);
    combined.insert(combined.end(), nonce.begin(), nonce.end());
    combined.insert(combined.end(), tag.begin(), tag.end());
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());
    
    return base64Encode(combined);
}

std::string Crypto::decrypt(const std::string& ciphertextB64) {
    if (!m_valid) return "";
    
    std::vector<uint8_t> combined = base64Decode(ciphertextB64);
    
    if (combined.size() < NONCE_SIZE + TAG_SIZE) return "";
    
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status = BCryptGenerateSymmetricKey(
        reinterpret_cast<BCRYPT_ALG_HANDLE>(m_hAlg),
        &hKey,
        nullptr,
        0,
        m_key.data(),
        KEY_SIZE,
        0
    );
    
    if (status != 0) return "";
    
    // Extract nonce, tag, ciphertext
    std::vector<uint8_t> nonce(combined.begin(), combined.begin() + NONCE_SIZE);
    std::vector<uint8_t> tag(combined.begin() + NONCE_SIZE, combined.begin() + NONCE_SIZE + TAG_SIZE);
    std::vector<uint8_t> ciphertext(combined.begin() + NONCE_SIZE + TAG_SIZE, combined.end());
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = NONCE_SIZE;
    authInfo.pbTag = tag.data();
    authInfo.cbTag = TAG_SIZE;
    
    ULONG plaintextSize = 0;
    status = BCryptDecrypt(
        hKey,
        ciphertext.data(),
        static_cast<ULONG>(ciphertext.size()),
        &authInfo,
        nullptr,
        0,
        nullptr,
        0,
        &plaintextSize,
        0
    );
    
    if (status != 0) {
        BCryptDestroyKey(hKey);
        return "";
    }
    
    std::vector<uint8_t> plaintext(plaintextSize);
    
    status = BCryptDecrypt(
        hKey,
        ciphertext.data(),
        static_cast<ULONG>(ciphertext.size()),
        &authInfo,
        nullptr,
        0,
        plaintext.data(),
        plaintextSize,
        &plaintextSize,
        0
    );
    
    BCryptDestroyKey(hKey);
    
    if (status != 0) return "";
    
    return std::string(plaintext.begin(), plaintext.end());
}

} // namespace GameAway
