#pragma once

#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace cow {

template <typename CharT, typename Traits = std::char_traits<CharT>>
    requires std::is_trivially_copyable_v<CharT> && std::is_trivially_destructible_v<CharT> &&
             std::is_default_constructible_v<CharT>
class cow_string {
public:
    static constexpr std::size_t sso_capacity = std::max<std::size_t>(16, sizeof(CharT)) / sizeof(CharT) - 1;

    cow_string() = default;

    cow_string(std::size_t count, CharT ch) { construct_fill(count, ch); }

    cow_string(const CharT* s, std::size_t count) { construct_from(s, count); }

    cow_string(const CharT* s) : cow_string(s, Traits::length(s)) {}

    cow_string(const cow_string& other) { init_from(other); }

    cow_string(cow_string&& other) noexcept { steal_from(std::move(other)); }

    ~cow_string() { release_heap(); }

    cow_string& operator=(const cow_string& other) {
        if (this == &other) {
            return *this;
        }
        if (block_ != nullptr && block_ == other.block_) {
            return *this;
        }
        if (mutable_leak_ && is_unique() && other.size() <= capacity()) {
            assign_range(other.chars(), other.size());
            return *this;
        }
        if (can_share_from(other)) {
            other.block_->refs.fetch_add(1, std::memory_order_relaxed);
            release_heap();
            block_ = other.block_;
            size_ = other.size_;
            mutable_leak_ = false;
            return *this;
        }
        assign_range(other.chars(), other.size_);
        return *this;
    }

    cow_string& operator=(cow_string&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        release_heap();
        steal_from(std::move(other));
        return *this;
    }

    cow_string& operator=(const CharT* s) {
        assign_range(s, Traits::length(s));
        return *this;
    }

    const CharT& operator[](std::size_t pos) const noexcept { return chars()[pos]; }

    CharT& operator[](std::size_t pos) {
        detach();
        mark_mutable_alias();
        return chars()[pos];
    }

    const CharT* data() const noexcept { return chars(); }
    const CharT* c_str() const noexcept { return chars(); }

    CharT* data() {
        detach();
        mark_mutable_alias();
        return chars();
    }

    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return block_ == nullptr ? sso_capacity : block_->capacity; }

    bool unique() const noexcept { return use_count() == 1; }

    std::size_t use_count() const noexcept {
        if (block_ == nullptr) {
            return 1;
        }
        return block_->refs.load(std::memory_order_relaxed);
    }

    void clear() noexcept {
        if (block_ != nullptr && !is_unique()) {
            release_heap();
            become_empty();
            return;
        }
        size_ = 0;
        chars()[0] = CharT{};
    }

    void swap(cow_string& other) noexcept {
        if (this == &other) {
            return;
        }
        swap_storage(other);
    }

    cow_string& append(const CharT* s, std::size_t count) {
        if (count == 0) {
            return *this;
        }
        if (points_inside(s, count)) {
            cow_string tmp(s, count);
            return append(tmp.c_str(), tmp.size());
        }
        if (count > max_size() - size_) {
            throw std::length_error("cow::cow_string::append");
        }
        ensure_writable(size_ + count);
        Traits::copy(chars() + size_, s, count);
        size_ += count;
        chars()[size_] = CharT{};
        return *this;
    }

    cow_string& append(const cow_string& other) { return append(other.chars(), other.size_); }

private:
    struct Shared {
        std::atomic<std::size_t> refs{1};
        std::size_t capacity = 0;
        CharT* data = nullptr;
    };

    static std::size_t max_size_value() noexcept {
        return std::numeric_limits<std::size_t>::max() / sizeof(CharT) - 1;
    }

    CharT local_[sso_capacity + 1]{};
    Shared* block_ = nullptr;
    std::size_t size_ = 0;
    bool mutable_leak_ = false;

    CharT* chars() noexcept { return block_ == nullptr ? local_ : block_->data; }
    const CharT* chars() const noexcept { return block_ == nullptr ? local_ : block_->data; }

    bool is_unique() const noexcept {
        return block_ == nullptr || block_->refs.load(std::memory_order_acquire) == 1;
    }

    bool can_share_from(const cow_string& other) const noexcept {
        return other.block_ != nullptr && !other.mutable_leak_;
    }

    void mark_mutable_alias() noexcept {
        if (block_ != nullptr) {
            mutable_leak_ = true;
        }
    }

    void become_empty() noexcept {
        block_ = nullptr;
        size_ = 0;
        mutable_leak_ = false;
        local_[0] = CharT{};
    }

    void release_heap() noexcept {
        if (block_ == nullptr) {
            return;
        }
        Shared* block = block_;
        block_ = nullptr;
        if (block->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            deallocate_block(block);
        }
    }

    std::size_t max_size() const noexcept { return max_size_value(); }

    bool points_inside(const CharT* src, std::size_t count) const noexcept {
        if (src == nullptr || count == 0) {
            return false;
        }
        const auto begin = reinterpret_cast<std::uintptr_t>(chars());
        const auto end = reinterpret_cast<std::uintptr_t>(chars() + size_ + 1);
        const auto s = reinterpret_cast<std::uintptr_t>(src);
        const auto e = reinterpret_cast<std::uintptr_t>(src + count);
        return s < end && begin < e;
    }

    Shared* allocate_block(std::size_t cap) {
        if (cap > max_size()) {
            throw std::length_error("cow::cow_string");
        }
        auto* shared = new Shared;
        try {
            shared->capacity = cap;
            shared->data = new CharT[cap + 1];
        } catch (...) {
            delete shared;
            throw;
        }
        return shared;
    }

    void deallocate_block(Shared* shared) noexcept {
        delete[] shared->data;
        delete shared;
    }

    void construct_from(const CharT* src, std::size_t count) {
        if (count > max_size()) {
            throw std::length_error("cow::cow_string");
        }
        if (count <= sso_capacity) {
            if (count != 0) {
                Traits::copy(local_, src, count);
            }
            local_[count] = CharT{};
            size_ = count;
            mutable_leak_ = false;
            return;
        }
        block_ = allocate_block(count);
        Traits::copy(block_->data, src, count);
        block_->data[count] = CharT{};
        size_ = count;
        mutable_leak_ = false;
    }

    void construct_fill(std::size_t count, CharT ch) {
        if (count > max_size()) {
            throw std::length_error("cow::cow_string");
        }
        if (count <= sso_capacity) {
            if (count != 0) {
                Traits::assign(local_, count, ch);
            }
            local_[count] = CharT{};
            size_ = count;
            mutable_leak_ = false;
            return;
        }
        block_ = allocate_block(count);
        Traits::assign(block_->data, count, ch);
        block_->data[count] = CharT{};
        size_ = count;
        mutable_leak_ = false;
    }

    void init_from(const cow_string& other) {
        if (can_share_from(other)) {
            block_ = other.block_;
            size_ = other.size_;
            mutable_leak_ = false;
            other.block_->refs.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        construct_from(other.chars(), other.size_);
    }

    void steal_from(cow_string&& other) noexcept {
        size_ = other.size_;
        if (other.block_ != nullptr) {
            block_ = other.block_;
            mutable_leak_ = other.mutable_leak_;
        } else {
            if (size_ != 0) {
                Traits::copy(local_, other.local_, size_);
            }
            local_[size_] = CharT{};
            block_ = nullptr;
            mutable_leak_ = false;
        }
        other.become_empty();
    }

    void swap_storage(cow_string& other) noexcept {
        if (block_ != nullptr && other.block_ != nullptr) {
            std::swap(block_, other.block_);
            std::swap(size_, other.size_);
            std::swap(mutable_leak_, other.mutable_leak_);
            return;
        }
        if (block_ == nullptr && other.block_ == nullptr) {
            const std::size_t n = size_ > other.size_ ? size_ : other.size_;
            for (std::size_t i = 0; i <= n; ++i) {
                std::swap(local_[i], other.local_[i]);
            }
            std::swap(size_, other.size_);
            return;
        }
        if (block_ == nullptr) {
            other.swap_storage(*this);
            return;
        }
        Shared* heap_block = block_;
        const std::size_t heap_size = size_;
        const bool leak = mutable_leak_;
        if (other.size_ != 0) {
            Traits::copy(local_, other.chars(), other.size_);
        }
        local_[other.size_] = CharT{};
        block_ = nullptr;
        size_ = other.size_;
        mutable_leak_ = false;

        other.block_ = heap_block;
        other.size_ = heap_size;
        other.mutable_leak_ = leak;
    }

    void assign_range(const CharT* src, std::size_t count) {
        if (count != 0 && points_inside(src, count)) {
            cow_string tmp(src, count);
            assign_range(tmp.c_str(), tmp.size());
            return;
        }
        if (count > max_size()) {
            throw std::length_error("cow::cow_string");
        }
        if (is_unique() && count <= capacity()) {
            if (count != 0) {
                Traits::copy(chars(), src, count);
            }
            chars()[count] = CharT{};
            size_ = count;
            return;
        }
        if (count <= sso_capacity) {
            if (count != 0) {
                Traits::copy(local_, src, count);
            }
            local_[count] = CharT{};
            release_heap();
            size_ = count;
            mutable_leak_ = false;
            return;
        }
        Shared* block = allocate_block(count);
        Traits::copy(block->data, src, count);
        block->data[count] = CharT{};
        release_heap();
        block_ = block;
        size_ = count;
        mutable_leak_ = false;
    }

    std::size_t grow_capacity(std::size_t min_cap) const {
        if (min_cap > max_size()) {
            throw std::length_error("cow::cow_string");
        }
        const std::size_t cap = capacity();
        std::size_t next = cap > max_size() / 2 ? max_size() : cap * 2;
        if (next < min_cap) {
            next = min_cap;
        }
        const std::size_t min_heap = std::max<std::size_t>(sso_capacity + 1, sizeof(CharT) < 16 ? 16 / sizeof(CharT) : 1);
        if (next < min_heap) {
            next = min_heap;
        }
        return next;
    }

    void reallocate_keeping_size(std::size_t new_cap) {
        if (new_cap < size_) {
            new_cap = size_;
        }
        if (new_cap <= sso_capacity) {
            if (block_ != nullptr) {
                if (size_ != 0) {
                    Traits::copy(local_, chars(), size_);
                }
                local_[size_] = CharT{};
                release_heap();
                mutable_leak_ = false;
            }
            return;
        }
        Shared* block = allocate_block(new_cap);
        if (size_ != 0) {
            Traits::copy(block->data, chars(), size_);
        }
        block->data[size_] = CharT{};
        release_heap();
        block_ = block;
        mutable_leak_ = false;
    }

    void ensure_writable(std::size_t min_size) {
        if (is_unique() && min_size <= capacity()) {
            return;
        }
        const std::size_t new_cap = min_size <= capacity() ? capacity() : grow_capacity(min_size);
        reallocate_keeping_size(new_cap);
    }

    void detach() {
        if (block_ == nullptr || is_unique()) {
            return;
        }
        reallocate_keeping_size(capacity());
    }
};

template <typename CharT, typename Traits>
int compare_chars(const CharT* lhs, std::size_t lhs_size, const CharT* rhs, std::size_t rhs_size) noexcept(
    noexcept(Traits::compare(lhs, rhs, lhs_size))) {
    const std::size_t n = rhs_size < lhs_size ? rhs_size : lhs_size;
    if (n != 0) {
        if (const int cmp = Traits::compare(lhs, rhs, n); cmp != 0) {
            return cmp;
        }
    }
    if (lhs_size < rhs_size) {
        return -1;
    }
    if (lhs_size > rhs_size) {
        return 1;
    }
    return 0;
}

template <typename CharT, typename Traits>
void swap(cow_string<CharT, Traits>& lhs, cow_string<CharT, Traits>& rhs) noexcept(
    noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template <typename CharT, typename Traits>
bool operator==(const cow_string<CharT, Traits>& lhs,
                const cow_string<CharT, Traits>& rhs) noexcept(
    noexcept(Traits::compare(lhs.data(), rhs.data(), lhs.size()))) {
    return lhs.size() == rhs.size() &&
           (lhs.size() == 0 || Traits::compare(lhs.data(), rhs.data(), lhs.size()) == 0);
}

template <typename CharT, typename Traits>
bool operator==(const cow_string<CharT, Traits>& lhs, const CharT* rhs) noexcept(
    noexcept(Traits::length(rhs)) && noexcept(Traits::compare(lhs.data(), rhs, lhs.size()))) {
    const std::size_t count = Traits::length(rhs);
    return compare_chars<CharT, Traits>(lhs.data(), lhs.size(), rhs, count) == 0;
}

template <typename CharT, typename Traits>
std::strong_ordering operator<=>(const cow_string<CharT, Traits>& lhs,
                                 const cow_string<CharT, Traits>& rhs) noexcept(
    noexcept(Traits::compare(lhs.data(), rhs.data(), lhs.size()))) {
    return compare_chars<CharT, Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

template <typename CharT, typename Traits>
std::strong_ordering operator<=>(const cow_string<CharT, Traits>& lhs, const CharT* rhs) noexcept(
    noexcept(Traits::length(rhs)) && noexcept(Traits::compare(lhs.data(), rhs, lhs.size()))) {
    return compare_chars<CharT, Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs)) <=> 0;
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(const cow_string<CharT, Traits>& lhs,
                                               const cow_string<CharT, Traits>& rhs) {
    cow_string<CharT, Traits> result(lhs);
    result.append(rhs);
    return result;
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(cow_string<CharT, Traits>&& lhs,
                                               const cow_string<CharT, Traits>& rhs) {
    lhs.append(rhs);
    return std::move(lhs);
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(const cow_string<CharT, Traits>& lhs, const CharT* rhs) {
    cow_string<CharT, Traits> result(lhs);
    result.append(rhs, Traits::length(rhs));
    return result;
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(cow_string<CharT, Traits>&& lhs, const CharT* rhs) {
    lhs.append(rhs, Traits::length(rhs));
    return std::move(lhs);
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(const CharT* lhs, const cow_string<CharT, Traits>& rhs) {
    cow_string<CharT, Traits> result(lhs);
    result.append(rhs);
    return result;
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(const cow_string<CharT, Traits>& lhs, CharT rhs) {
    cow_string<CharT, Traits> result(lhs);
    result.append(&rhs, 1);
    return result;
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(cow_string<CharT, Traits>&& lhs, CharT rhs) {
    lhs.append(&rhs, 1);
    return std::move(lhs);
}

template <typename CharT, typename Traits>
cow_string<CharT, Traits> operator+(CharT lhs, const cow_string<CharT, Traits>& rhs) {
    cow_string<CharT, Traits> result(1, lhs);
    result.append(rhs);
    return result;
}

using string = cow_string<char>;

}