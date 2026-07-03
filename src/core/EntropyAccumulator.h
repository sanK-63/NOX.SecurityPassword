#pragma once

#include "core/SecureBuffer.h"
#include <cstddef>
#include <cstdint>

class EntropyAccumulator {
public:
    static constexpr size_t REQUIRED_ENTROPY_BYTES = 1024;

    EntropyAccumulator() noexcept;

    bool addEvent(int x, int y);
    bool isReady() const noexcept;
    [[nodiscard]] float getProgress() const noexcept;
    SecureBuffer getFinalSeed();
    void reset() noexcept;

private:
    SecureBuffer m_pool;
};
