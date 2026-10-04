#pragma once


#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

template <typename T, std::size_t N>
    requires(N > 0)
class SquareMatrix {
public:    
    static constexpr std::size_t order = N;

    SquareMatrix() = default;

    SquareMatrix(const std::array<std::array<T, N>, N>& rows)
    : data_(rows) {}

    SquareMatrix(const T (&rows)[N][N]) {
        for (std::size_t row = 0; row < N; ++row) {
            for (std::size_t col = 0; col < N; ++col) {
                data_[row][col] = rows[row][col];
            }
        }
    }

    static constexpr SquareMatrix identity() {
        SquareMatrix result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data_[i][i] = T{1};
        }

        return result;
    }

    T& operator()(std::size_t row, std::size_t col) {
        return data_[row][col];
    }

    const T& operator()(std::size_t row, std::size_t col) const {
        return data_[row][col];
    }

    constexpr SquareMatrix inverse() const {
        std::array<std::array<T, 2 * N>, N> aug{};
        for (std::size_t row = 0; row < N; ++row) {
            for (std::size_t col = 0; col < N; ++col) {
                aug[row][col] = data_[row][col];
            }
            aug[row][N + row] = T{1};
        }

        for (std::size_t col = 0; col < N; ++col) {
            std::size_t pivot = col;
            for (std::size_t row = col + 1; row < N; ++row) {
                if (abs_value(aug[row][col]) > abs_value(aug[pivot][col])) {
                    pivot = row;
                }
            }
            if (is_zero(aug[pivot][col])) {
                throw std::domain_error("SquareMatrix::inverse: singular matrix");
            }
            if (pivot != col) {
                using std::swap;
                swap(aug[pivot], aug[col]);
            }

            const T pivot_value = aug[col][col];
            for (std::size_t j = col; j < 2 * N; ++j) {
                aug[col][j] /= pivot_value;
            }

            for (std::size_t row = 0; row < N; ++row) {
                if (row == col) {
                    continue;
                }
                const T factor = aug[row][col];
                for (std::size_t j = col; j < 2 * N; ++j) {
                    aug[row][j] -= factor * aug[col][j];
                }
            }
        }

        SquareMatrix result;
        for (std::size_t row = 0; row < N; ++row) {
            for (std::size_t col = 0; col < N; ++col) {
                result.data_[row][col] = aug[row][N + col];
            }
        }
        return result;
    }

    SquareMatrix& operator*=(const SquareMatrix &rhs) {
        SquareMatrix product;
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t k = 0; k < N; ++k) {
                const T factor = data_[i][k];
                for (std::size_t j = 0; j < N; ++j) {
                    product.data_[i][j] += factor * rhs.data_[k][j];
                }
            }
        }
        data_ = std::move(product.data_);
        return *this;
    }

    constexpr SquareMatrix operator*(const SquareMatrix& rhs) const {
        SquareMatrix result = *this;
        result *= rhs;
        return result;
    }

    //bool operator==(const SquareMatrix&, const SquareMatrix&) = default;
    bool operator==(const SquareMatrix& other) const = default;

private:
    std::array<std::array<T, N>, N> data_{};

    constexpr T abs_value(T value) const {
        return value < T{0} ? -value : value;
    }

    constexpr bool is_zero(T value) const {
        return value == T{0};
    }
};


