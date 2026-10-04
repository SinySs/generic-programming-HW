#pragma once

#include <complex>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
concept Integer = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;


template <typename T>
concept Real = std::same_as<std::remove_cv_t<T>, double>;

template <typename M>
concept ExponentiableMatrix = requires(M& lhs, M& rhs) {
    { M::identity() } -> std::same_as<M>;
    { lhs *= rhs } -> std::same_as<M&>;
};

template <typename M>
concept InversibleMatrix = ExponentiableMatrix<M> && requires(const M& m) {
    { m.inverse() } -> std::same_as<M>;
};

template <typename T>
constexpr T nth_power(T x, T acc, unsigned long long n) {
    if (n == 0) {
        return acc;
    }
    if (n == 1 || x == acc) {
        return x;
    }

    while (n > 0) {
        if ((n & 1ull) == 1ull) {
            acc *= x;
            n -= 1;
        } else {
            x *= x;
            n /= 2;
        }
    }
    return acc;
}

template <Integer Base, Integer Exp>
constexpr Base nth_power(Base base, Exp exp) {
    if (std::signed_integral<Exp>) {
        if (exp < 0) {
            throw std::domain_error("nth_power negative exp, use non int type");
        }    
    }

    return nth_power(base, Base{1}, static_cast<unsigned long long>(exp));
}    

template <Real Base, Integer Exp>
constexpr Base nth_power(Base base, Exp exp) {
    if (exp < 0) {
        return 1 / nth_power(base, Base{1}, static_cast<unsigned long long>(std::abs(exp)));
    } else {
        return nth_power(base, Base{1}, static_cast<unsigned long long>(exp));
    }
}

template <Real Base, Integer Exp>
constexpr std::complex<Base> nth_power(std::complex<Base> base, Exp exp) {
    using Complex = std::complex<Base>;
    if (exp < 0) {
        return Complex{Base{1}} / nth_power(base, Complex{Base{1}}, static_cast<unsigned long long>(std::abs(exp)));
    } else {
        return nth_power(base, Complex{Base{1}}, static_cast<unsigned long long>(exp));
    }
}

template <ExponentiableMatrix M, Integer Exp>
M nth_power(M base, Exp exp) {
    if (exp < 0) {
        if constexpr (InversibleMatrix<M>) {
            return nth_power(base.inverse(), M::identity(), static_cast<unsigned long long>(-exp));
        } else {
            throw std::domain_error(
                "nth_power: negative exponent requires an invertible matrix");
        }        
    } else {
        const unsigned long long n = static_cast<unsigned long long>(exp);

        return nth_power(base, M::identity(), n);
    }
}