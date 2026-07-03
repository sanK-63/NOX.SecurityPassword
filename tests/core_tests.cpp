#include <cassert>
#include <cstring>
#include <iostream>
#include <sodium.h>
#include "core/SecureBuffer.h"
#include "core/EntropyAccumulator.h"
#include "core/StatelessGenerator.h"

static void test_secure_buffer_default()
{
    SecureBuffer buf;
    assert(buf.size() == 0);
    assert(buf.empty());
    assert(buf.data() == nullptr);
    std::cout << "[PASS] SecureBuffer default\n";
}

static void test_secure_buffer_size()
{
    SecureBuffer buf(64);
    assert(buf.size() == 64);
    assert(!buf.empty());
    assert(buf.data() != nullptr);
    std::cout << "[PASS] SecureBuffer size\n";
}

static void test_secure_buffer_fill()
{
    const unsigned char expected[] = "hello";
    SecureBuffer buf(expected, sizeof(expected));
    assert(buf.size() == sizeof(expected));
    assert(std::memcmp(buf.data(), expected, sizeof(expected)) == 0);
    std::cout << "[PASS] SecureBuffer fill\n";
}

static void test_secure_buffer_move()
{
    SecureBuffer a(16);
    void* old_ptr = a.data();
    SecureBuffer b = std::move(a);
    assert(b.size() == 16);
    assert(b.data() == old_ptr);
    assert(a.empty());
    assert(a.data() == nullptr);
    (void)old_ptr;
    std::cout << "[PASS] SecureBuffer move\n";
}

static void test_secure_buffer_clear()
{
    SecureBuffer buf(32);
    randombytes_buf(buf.data(), buf.size());
    buf.clear();
    assert(buf.empty());
    assert(buf.size() == 0);
    assert(buf.data() == nullptr);
    std::cout << "[PASS] SecureBuffer clear\n";
}

static void test_secure_buffer_resize()
{
    SecureBuffer buf(16);
    assert(buf.size() == 16);
    buf.resize(32);
    assert(buf.size() == 32);
    buf.resize(8);
    assert(buf.size() == 8);
    buf.resize(0);
    assert(buf.empty());
    std::cout << "[PASS] SecureBuffer resize\n";
}

static void test_secure_buffer_append()
{
    SecureBuffer buf;
    const unsigned char a[] = "hello";
    const unsigned char b[] = " world";
    buf.append(a, sizeof(a) - 1);
    buf.append(b, sizeof(b) - 1);
    assert(buf.size() == 11);
    assert(std::memcmp(buf.data(), "hello world", 11) == 0);
    std::cout << "[PASS] SecureBuffer append\n";
}

static void test_entropy_accumulator()
{
    EntropyAccumulator acc;
    assert(!acc.isReady());

    for (int i = 0; i < 300; i++) {
        if (acc.addEvent(i * 7, i * 13)) break;
    }
    assert(acc.isReady());

    SecureBuffer seed = acc.getFinalSeed();
    assert(seed.size() == crypto_generichash_BYTES);
    assert(!seed.empty());
    std::cout << "[PASS] EntropyAccumulator\n";
}

static void test_stateless_generator()
{
    StatelessGenerator gen;
    const std::string alphabet =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    std::string p1 = gen.generate("master", "example.com", "user", 16, alphabet);
    assert(p1.size() == 16);

    // Same inputs → same output
    std::string p2 = gen.generate("master", "example.com", "user", 16, alphabet);
    assert(p1 == p2);

    // Different master → different output
    std::string p3 = gen.generate("different", "example.com", "user", 16, alphabet);
    assert(p1 != p3);

    // Different domain → different output
    std::string p4 = gen.generate("master", "other.com", "user", 16, alphabet);
    assert(p1 != p4);

    std::cout << "[PASS] StatelessGenerator deterministic\n";
}

int main()
{
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    test_secure_buffer_default();
    test_secure_buffer_size();
    test_secure_buffer_fill();
    test_secure_buffer_move();
    test_secure_buffer_clear();
    test_secure_buffer_resize();
    test_secure_buffer_append();
    test_entropy_accumulator();
    test_stateless_generator();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
