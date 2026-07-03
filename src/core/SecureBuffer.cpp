#include "core/SecureBuffer.h"
#include <sodium.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

SecureBuffer::SecureBuffer() noexcept
    : m_data(nullptr), m_size(0) {}

SecureBuffer::SecureBuffer(size_t size)
    : m_data(nullptr), m_size(0)
{
    if (size == 0) return;
    m_data = static_cast<unsigned char*>(sodium_malloc(size));
    if (!m_data) {
        throw std::bad_alloc();
    }
    m_size = size;
    sodium_mlock(m_data, m_size);
}

SecureBuffer::SecureBuffer(const void* data, size_t size)
    : SecureBuffer(size)
{
    if (size > 0 && data != nullptr) {
        std::memcpy(m_data, data, size);
    }
}

SecureBuffer::~SecureBuffer()
{
    free_memory();
}

SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : m_data(other.m_data), m_size(other.m_size)
{
    other.m_data = nullptr;
    other.m_size = 0;
}

SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept
{
    if (this != &other) {
        free_memory();
        m_data = other.m_data;
        m_size = other.m_size;
        other.m_data = nullptr;
        other.m_size = 0;
    }
    return *this;
}

void SecureBuffer::assign(const void* data, size_t size)
{
    if (data == nullptr || size == 0) {
        clear();
        return;
    }

    if (m_size != size) {
        auto* new_data = static_cast<unsigned char*>(sodium_malloc(size));
        if (!new_data) {
            throw std::bad_alloc();
        }
        sodium_mlock(new_data, size);
        std::memcpy(new_data, data, size);
        free_memory();
        m_data = new_data;
        m_size = size;
    } else {
        std::memcpy(m_data, data, size);
    }
}

void SecureBuffer::resize(size_t new_size)
{
    if (new_size == 0) {
        clear();
        return;
    }

    if (new_size == m_size) return;

    auto* new_data = static_cast<unsigned char*>(sodium_malloc(new_size));
    if (!new_data) {
        throw std::bad_alloc();
    }
    sodium_mlock(new_data, new_size);

    size_t copy_size = std::min(m_size, new_size);
    if (m_data && copy_size > 0) {
        std::memcpy(new_data, m_data, copy_size);
    }

    free_memory();
    m_data = new_data;
    m_size = new_size;
}

void SecureBuffer::append(const void* data, size_t size)
{
    if (data == nullptr || size == 0) return;

    size_t new_size = m_size + size;
    auto* new_data = static_cast<unsigned char*>(sodium_malloc(new_size));
    if (!new_data) {
        throw std::bad_alloc();
    }
    sodium_mlock(new_data, new_size);

    if (m_data && m_size > 0) {
        std::memcpy(new_data, m_data, m_size);
    }
    std::memcpy(new_data + m_size, data, size);

    free_memory();
    m_data = new_data;
    m_size = new_size;
}

void SecureBuffer::clear() noexcept
{
    free_memory();
    m_data = nullptr;
    m_size = 0;
}

size_t SecureBuffer::size() const noexcept
{
    return m_size;
}

bool SecureBuffer::empty() const noexcept
{
    return m_size == 0;
}

const unsigned char* SecureBuffer::data() const noexcept
{
    return m_data;
}

unsigned char* SecureBuffer::data() noexcept
{
    return m_data;
}

void SecureBuffer::free_memory() noexcept
{
    if (m_data) {
        sodium_munlock(m_data, m_size);
        sodium_free(m_data);
        m_data = nullptr;
        m_size = 0;
    }
}
