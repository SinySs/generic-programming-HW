#include <cassert>
#include <limits>

constexpr bool add_would_overflow(int lhs, int rhs) noexcept {
    if (rhs > 0) {
        return lhs > std::numeric_limits<int>::max() - rhs;
    }
    if (rhs < 0) {
        return lhs < std::numeric_limits<int>::min() - rhs;
    }
    return false;
}

int foo(int* a, int base, int off) {
    if (a == nullptr || add_would_overflow(base, off)) {
        return 42;
    }
    const int index = base + off;
    if (index < 0) {
        return 42;
    }
    return a[index];
}

int main() {
    int data[]{10, 20, 30};

    const int int_max = std::numeric_limits<int>::max();
    const int int_min = std::numeric_limits<int>::min();

    int base = 0;
    int off = 0;

    base = 0;  off = 0;   assert(foo(data, base, off) == 10);
    base = 0;  off = 2;   assert(foo(data, base, off) == 30);
    base = 2;  off = -2;  assert(foo(data, base, off) == 10);
    base = -1; off = 1;   assert(foo(data, base, off) == 10);
    base = -1; off = 2;   assert(foo(data, base, off) == 20);
    base = -1; off = 3;   assert(foo(data, base, off) == 30);

    base = 0;  off = -1;  assert(foo(data, base, off) == 42);
    base = -1; off = 0;   assert(foo(data, base, off) == 42);
    base = -5; off = 3;   assert(foo(data, base, off) == 42);
    base = int_min; off = int_max; assert(foo(data, base, off) == 42);
    base = int_max; off = int_min; assert(foo(data, base, off) == 42);

    base = int_max; off = 1;       assert(foo(data, base, off) == 42);
    base = int_min; off = -1;      assert(foo(data, base, off) == 42);
    base = int_max; off = int_max; assert(foo(data, base, off) == 42);
    base = int_min; off = int_min; assert(foo(data, base, off) == 42);

    base = 0;  off = 0;   assert(foo(nullptr, base, off) == 42);
    base = 0;  off = 2;   assert(foo(nullptr, base, off) == 42);
    base = int_max; off = 1;  assert(foo(nullptr, base, off) == 42);
    base = int_min; off = -1; assert(foo(nullptr, base, off) == 42);

    return 0;
}

