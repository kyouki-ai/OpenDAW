#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace opendaw
{
template <typename T, std::size_t Capacity> class SpscQueue
{
    static_assert(Capacity > 1 && std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);

  public:
    bool push(T value) noexcept
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = (write + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire))
            return false;
        values_[write] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T &value) noexcept
    {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire))
            return false;
        value = values_[read];
        read_.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }

  private:
    std::array<T, Capacity> values_{};
    std::atomic<std::size_t> read_{0};
    std::atomic<std::size_t> write_{0};
};
} // namespace opendaw
