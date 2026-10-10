#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

/**
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer template.
 * @tparam T Element type (typically uint8_t for serial bytes).
 * @tparam Capacity Must be a power of 2 for fast bitmask wrap-around.
 */
template <typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity > 0) && ((Capacity & (Capacity - 1)) == 0),
                  "SpscRingBuffer Capacity must be a power of 2");

public:
    constexpr SpscRingBuffer() : head_(0), tail_(0) {}

    /**
     * @brief Push a single element (Producer).
     * @param item Element to insert.
     * @return true if successful, false if buffer is full.
     */
    bool push(const T& item) {
        const size_t current_head = head_;
        const size_t next_head = (current_head + 1) & BufferMask;
        if (next_head == tail_) {
            return false; // Buffer full
        }
        buffer_[current_head] = item;
        head_ = next_head;
        return true;
    }

    /**
     * @brief Write multiple elements (Producer).
     * @param data Pointer to input array.
     * @param len Number of elements to write.
     * @return Number of elements actually written.
     */
    size_t write(const T* data, size_t len) {
        if (!data || len == 0) {
            return 0;
        }
        size_t written = 0;
        while (written < len) {
            const size_t current_head = head_;
            const size_t next_head = (current_head + 1) & BufferMask;
            if (next_head == tail_) {
                break; // Full
            }
            buffer_[current_head] = data[written];
            head_ = next_head;
            written++;
        }
        return written;
    }

    /**
     * @brief Pop a single element (Consumer).
     * @param item Output destination.
     * @return true if successful, false if buffer is empty.
     */
    bool pop(T& item) {
        const size_t current_tail = tail_;
        if (current_tail == head_) {
            return false; // Buffer empty
        }
        item = buffer_[current_tail];
        tail_ = (current_tail + 1) & BufferMask;
        return true;
    }

    /**
     * @brief Read multiple elements (Consumer).
     * @param dest Pointer to destination array.
     * @param max_len Maximum number of elements to read.
     * @return Number of elements actually read.
     */
    size_t read(T* dest, size_t max_len) {
        if (!dest || max_len == 0) {
            return 0;
        }
        size_t count = 0;
        while (count < max_len) {
            const size_t current_tail = tail_;
            if (current_tail == head_) {
                break; // Empty
            }
            dest[count] = buffer_[current_tail];
            tail_ = (current_tail + 1) & BufferMask;
            count++;
        }
        return count;
    }

    /**
     * @brief Peek at the next element without removing it.
     * @param item Output destination.
     * @return true if an element is available, false if empty.
     */
    bool peek(T& item) const {
        if (head_ == tail_) {
            return false;
        }
        item = buffer_[tail_];
        return true;
    }

    /**
     * @brief Number of elements currently in the buffer.
     */
    size_t available() const {
        return (head_ - tail_) & BufferMask;
    }

    /**
     * @brief Number of free slots remaining in the buffer.
     */
    size_t free_space() const {
        return (Capacity - 1) - available();
    }

    /**
     * @brief Check if buffer is empty.
     */
    bool is_empty() const {
        return head_ == tail_;
    }

    /**
     * @brief Check if buffer is full.
     */
    bool is_full() const {
        return ((head_ + 1) & BufferMask) == tail_;
    }

    /**
     * @brief Reset buffer to empty state.
     */
    void clear() {
        head_ = 0;
        tail_ = 0;
    }

private:
    static constexpr size_t BufferMask = Capacity - 1;
    std::array<T, Capacity> buffer_{};
    volatile size_t head_{0};
    volatile size_t tail_{0};
};

