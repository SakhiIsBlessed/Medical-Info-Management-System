#pragma once
// Security helpers: salted password hashing, stream-cipher encryption, hex utils.
// NOTE: These are EDUCATIONAL implementations. For production use a vetted library
// (libsodium / OpenSSL: Argon2id or bcrypt for passwords, AES-256-GCM for data).
#include <string>
#include <cstdint>
#include <random>

namespace security {

inline uint64_t fnv1a(const std::string& s, uint64_t h = 1469598103934665603ULL) {
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

inline std::string toHex(const std::string& in) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) { out += d[c >> 4]; out += d[c & 15]; }
    return out;
}

inline bool fromHex(const std::string& in, std::string& out) {
    if (in.size() % 2) return false;
    out.clear();
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < in.size(); i += 2) {
        int a = val(in[i]), b = val(in[i + 1]);
        if (a < 0 || b < 0) return false;
        out += static_cast<char>((a << 4) | b);
    }
    return true;
}

inline std::string u64Hex(uint64_t v) {
    std::string s(8, '\0');
    for (int i = 0; i < 8; ++i) s[i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    return toHex(s);
}

inline std::string randomHex(size_t bytes) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::string raw;
    for (size_t i = 0; i < bytes; ++i) raw += static_cast<char>(gen() & 0xFF);
    return toHex(raw);
}

// Salted, iterated hash (key stretching) -> 128-bit hex digest.
inline std::string hashPassword(const std::string& pw, const std::string& salt) {
    uint64_t a = fnv1a(salt + pw), b = fnv1a(pw + salt, 0x84222325cbf29ce4ULL);
    for (int i = 0; i < 50000; ++i) {
        a = fnv1a(u64Hex(b) + salt, a);
        b = fnv1a(u64Hex(a) + pw, b);
    }
    return u64Hex(a) + u64Hex(b);
}

inline uint64_t splitmix(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Symmetric stream cipher (XOR with key-derived keystream). Same call encrypts/decrypts.
inline std::string streamCipher(std::string data, const std::string& key, const std::string& nonce) {
    uint64_t st = fnv1a(key + nonce);
    for (int i = 0; i < 1000; ++i) splitmix(st);
    size_t i = 0;
    while (i < data.size()) {
        uint64_t r = splitmix(st);
        for (int b = 0; b < 8 && i < data.size(); ++b, ++i)
            data[i] = static_cast<char>(data[i] ^ static_cast<char>((r >> (8 * b)) & 0xFF));
    }
    return data;
}

inline std::string encrypt(const std::string& plain, const std::string& passphrase) {
    std::string nonce = randomHex(8);
    return "MIS1\n" + nonce + "\n" + toHex(streamCipher("MISOK\n" + plain, passphrase, nonce));
}

// Returns false if format is bad or passphrase is wrong.
inline bool decrypt(const std::string& blob, const std::string& passphrase, std::string& plain) {
    if (blob.compare(0, 5, "MIS1\n") != 0) return false;
    size_t nl = blob.find('\n', 5);
    if (nl == std::string::npos) return false;
    std::string nonce = blob.substr(5, nl - 5), hex = blob.substr(nl + 1), raw;
    while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r')) hex.pop_back();
    if (!fromHex(hex, raw)) return false;
    std::string dec = streamCipher(raw, passphrase, nonce);
    if (dec.compare(0, 6, "MISOK\n") != 0) return false;
    plain = dec.substr(6);
    return true;
}

} // namespace security
