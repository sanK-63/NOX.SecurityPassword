#include <iostream>
#include <sodium.h>
#include "core/SecureBuffer.h"
#include "core/EntropyAccumulator.h"
#include "core/StatelessGenerator.h"

static void print_hex(const SecureBuffer& buf)
{
    for (size_t i = 0; i < buf.size(); ++i) {
        printf("%02x", buf.data()[i]);
    }
    printf("\n");
}

static void demo_secure_buffer()
{
    std::cout << "--- SecureBuffer ---\n";
    SecureBuffer buf(32);
    randombytes_buf(buf.data(), buf.size());
    std::cout << "32 random bytes:\n  ";
    print_hex(buf);

    SecureBuffer moved = std::move(buf);
    std::cout << "After move — original empty? "
              << (buf.empty() ? "yes" : "no") << "\n\n";
}

static void demo_entropy_accumulator()
{
    std::cout << "--- EntropyAccumulator ---\n";
    EntropyAccumulator acc;

    // Simulate 200 mouse events (more than enough for 1024 bytes)
    for (int i = 0; i < 200; i++) {
        int x = (i * 137 + 43) % 1920;
        int y = (i * 251 + 17) % 1080;
        if (acc.addEvent(x, y)) break;
    }

    std::cout << "Pool filled: " << acc.isReady() << "\n";
    SecureBuffer seed = acc.getFinalSeed();
    std::cout << "Final seed (" << seed.size() << " bytes):\n  ";
    print_hex(seed);
    std::cout << "\n";
}

static void demo_stateless_generator()
{
    std::cout << "--- StatelessGenerator ---\n";
    StatelessGenerator gen;

    const std::string alphabet =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()_+-=[]{}|;':\",./<>?";

    std::string password = gen.generate(
        "MySecureMasterPassword123!",
        "github.com",
        "user@example.com",
        32,
        alphabet);

    std::cout << "Password (32 chars, deterministic):\n  " << password << "\n";

    // Same inputs with key=0 matches default
    std::string password_key0 = gen.generate(
        "MySecureMasterPassword123!",
        "github.com",
        "user@example.com",
        32,
        alphabet,
        0);
    std::cout << "With key=0:\n  " << password_key0 << "\n";
    std::cout << "Match (key=0 vs default): " << (password == password_key0 ? "yes" : "no") << "\n";

    // Different key → different password
    std::string password_key1 = gen.generate(
        "MySecureMasterPassword123!",
        "github.com",
        "user@example.com",
        32,
        alphabet,
        1);
    std::cout << "With key=1:\n  " << password_key1 << "\n";
    std::cout << "Different from key=0: " << (password != password_key1 ? "yes" : "no") << "\n";

    // Re-generated with same key → same password
    std::string password2 = gen.generate(
        "MySecureMasterPassword123!",
        "github.com",
        "user@example.com",
        32,
        alphabet);

    std::cout << "Re-generated:\n  " << password2 << "\n";
    std::cout << "Match: " << (password == password2 ? "yes" : "no") << "\n\n";
}

int main()
{
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    demo_secure_buffer();
    demo_entropy_accumulator();
    demo_stateless_generator();

    std::cout << "All demos completed.\n";
    return 0;
}
