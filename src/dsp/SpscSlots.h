// SPDX-License-Identifier: GPL-3.0-or-later
//
// SpscSlots: a lock-free queue of preallocated slots between exactly one
// producer thread and one consumer thread.
//
// It carries sample blocks from the device thread to the DSP thread, and
// spectrum rows from the DSP thread to the UI. Slots are allocated once, in
// the constructor; afterwards neither side allocates, locks or waits:
//
//   producer:  if (T* slot = q.beginWrite()) { fill *slot; q.commitWrite(); }
//              else { the consumer is behind: drop and count }
//   consumer:  while (T* slot = q.beginRead()) { use *slot; q.commitRead(); }
//
// The head and tail counters only grow; `head - tail` is the number of
// filled slots. Each counter is written by one thread and read by the other,
// with release/acquire ordering so slot contents are visible before the
// counter that publishes them.

#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <new>
#include <vector>

namespace zarescope::dsp {

template <typename T>
class SpscSlots {
public:
    // `capacity` is rounded up to a power of two. `init` prepares each slot
    // (for example, reserves a buffer) before any thread uses the queue.
    template <typename Init>
    SpscSlots(size_t capacity, Init&& init) : m_slots(std::bit_ceil(capacity < 2 ? 2 : capacity)) {
        m_mask = m_slots.size() - 1;
        for (T& slot : m_slots) {
            init(slot);
        }
    }
    explicit SpscSlots(size_t capacity) : SpscSlots(capacity, [](T&) {}) {}

    SpscSlots(const SpscSlots&) = delete;
    SpscSlots& operator=(const SpscSlots&) = delete;
    SpscSlots(SpscSlots&&) = delete;
    SpscSlots& operator=(SpscSlots&&) = delete;
    ~SpscSlots() = default;

    // Producer: the next free slot, or nullptr when the queue is full.
    T* beginWrite() {
        const size_t head = m_head.load(std::memory_order_relaxed);
        if (head - m_tail.load(std::memory_order_acquire) == m_slots.size()) {
            return nullptr;
        }
        return &m_slots[head & m_mask];
    }
    // Producer: publishes the slot returned by beginWrite().
    void commitWrite() {
        m_head.store(m_head.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    // Consumer: the oldest filled slot, or nullptr when the queue is empty.
    T* beginRead() {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (m_head.load(std::memory_order_acquire) == tail) {
            return nullptr;
        }
        return &m_slots[tail & m_mask];
    }
    // Consumer: hands the slot returned by beginRead() back to the producer.
    void commitRead() {
        m_tail.store(m_tail.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    [[nodiscard]] size_t capacity() const { return m_slots.size(); }
    // Filled slots right now; exact only when both threads are idle.
    [[nodiscard]] size_t sizeApprox() const {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
    }

private:
    // Keep the two counters on separate cache lines so the threads don't
    // fight over one line.
    static constexpr size_t kCacheLine = 64;

    std::vector<T> m_slots;
    size_t m_mask = 0;
    alignas(kCacheLine) std::atomic<size_t> m_head{0};  // written by the producer
    alignas(kCacheLine) std::atomic<size_t> m_tail{0};  // written by the consumer
};

}  // namespace zarescope::dsp
