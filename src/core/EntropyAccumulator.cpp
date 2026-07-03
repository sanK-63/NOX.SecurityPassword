#include "core/EntropyAccumulator.h"
#include <sodium.h>
#include <chrono>

EntropyAccumulator::EntropyAccumulator() noexcept {}

bool EntropyAccumulator::addEvent(int x, int y)
{
    if (m_pool.size() >= REQUIRED_ENTROPY_BYTES) {
        return true;
    }

    auto now = std::chrono::high_resolution_clock::now()
                   .time_since_epoch()
                   .count();

    m_pool.append(&x, sizeof(x));
    m_pool.append(&y, sizeof(y));
    m_pool.append(&now, sizeof(now));

    return m_pool.size() >= REQUIRED_ENTROPY_BYTES;
}

float EntropyAccumulator::getProgress() const noexcept
{
    return static_cast<float>(m_pool.size()) / static_cast<float>(REQUIRED_ENTROPY_BYTES) * 100.0f;
}

bool EntropyAccumulator::isReady() const noexcept
{
    return m_pool.size() >= REQUIRED_ENTROPY_BYTES;
}

SecureBuffer EntropyAccumulator::getFinalSeed()
{
    SecureBuffer seed(crypto_generichash_BYTES);
    crypto_generichash(
        seed.data(), seed.size(),
        m_pool.data(), m_pool.size(),
        nullptr, 0);
    m_pool.clear();
    return seed;
}

void EntropyAccumulator::reset() noexcept
{
    m_pool.clear();
}
