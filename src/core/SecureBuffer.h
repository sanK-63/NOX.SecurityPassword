#pragma once

#include <cstddef>
#include <cstdint>

class SecureBuffer {
public:
    SecureBuffer() noexcept;
    explicit SecureBuffer(size_t size);
    SecureBuffer(const void* data, size_t size);
    ~SecureBuffer();

    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    void assign(const void* data, size_t size);
    void append(const void* data, size_t size);
    void resize(size_t new_size);
    void clear() noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] const unsigned char* data() const noexcept;
    [[nodiscard]] unsigned char* data() noexcept;

private:
    void free_memory() noexcept;

    unsigned char* m_data = nullptr;
    size_t m_size = 0;
};
