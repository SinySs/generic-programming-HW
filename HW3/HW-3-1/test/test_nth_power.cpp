#include "nth_power.hpp"
#include "square_matrix.hpp"

#include "gtest/gtest.h"

TEST(NthPowerIntegers, ExactPowers) {
    EXPECT_EQ(nth_power(2, 10), 1024);
    EXPECT_EQ(nth_power(-2, 9), -512);
    EXPECT_EQ(nth_power(0.5, 2), 0.25);
    EXPECT_EQ(nth_power(0.5, -2), 4);
    EXPECT_EQ(nth_power(-0.5, -2), 4);
    EXPECT_EQ(nth_power(-0.5, -1), -2);

    EXPECT_THROW((void)nth_power(2, -3), std::domain_error);
}

TEST(NthPowerComplex, PowersOfI) {
    using C = std::complex<double>;
    const C i(0.0, 1.0);

    EXPECT_EQ(nth_power(i, 0), C(1.0, 0.0));
    EXPECT_EQ(nth_power(i, 1), i);
    EXPECT_EQ(nth_power(i, 2), C(-1.0, 0.0));
    EXPECT_EQ(nth_power(i, 3), C(0.0, -1.0));
    EXPECT_EQ(nth_power(i, 4), C(1.0, 0.0));
    EXPECT_EQ(nth_power(i, 5), i);
    EXPECT_EQ(nth_power(i, -1), C(0.0, -1.0));
    EXPECT_EQ(nth_power(i, -2), C(-1.0, 0.0));
}

TEST(NthPowerMatrices, ClosedForm) {
    int shear_rows[2][2] = {{1, 1}, {0, 1}};
    SquareMatrix<int, 2> shear{shear_rows};
    const auto shear4 = nth_power(shear, 4);
    EXPECT_EQ(shear4(0, 0), 1);
    EXPECT_EQ(shear4(0, 1), 4);
    EXPECT_EQ(shear4(1, 0), 0);
    EXPECT_EQ(shear4(1, 1), 1);
    EXPECT_EQ(nth_power(shear, 0), (SquareMatrix<int, 2>::identity()));
    EXPECT_EQ(nth_power(shear, 1), shear);

    int lower_rows[2][2] = {{1, 0}, {1, 1}};
    SquareMatrix<int, 2> lower{lower_rows};
    const auto lower5 = nth_power(lower, 5);
    EXPECT_EQ(lower5(1, 0), 5);
    EXPECT_EQ(lower5(0, 1), 0);

    int rotation_rows[2][2] = {{0, -1}, {1, 0}};
    SquareMatrix<int, 2> rotation{rotation_rows};
    int minus_identity_rows[2][2] = {{-1, 0}, {0, -1}};
    EXPECT_EQ(nth_power(rotation, 2), (SquareMatrix<int, 2>{minus_identity_rows}));
    EXPECT_EQ(nth_power(rotation, 4), (SquareMatrix<int, 2>::identity()));

    int diagonal_rows[2][2] = {{2, 0}, {0, 3}};
    SquareMatrix<int, 2> diagonal{diagonal_rows};
    const auto diagonal3 = nth_power(diagonal, 3);
    EXPECT_EQ(diagonal3(0, 0), 8);
    EXPECT_EQ(diagonal3(1, 1), 27);
    EXPECT_EQ(diagonal3(0, 1), 0);

    SquareMatrix<int, 2> zero{};
    EXPECT_EQ(nth_power(zero, 0),(SquareMatrix<int, 2>::identity()));
    EXPECT_EQ(nth_power(zero, 3), zero);

    int scalar_rows[1][1] = {{2}};
    SquareMatrix<int, 1> scalar{scalar_rows};
    EXPECT_EQ(nth_power(scalar, 5)(0, 0), 32);

    int nilpotent_rows[3][3] = {{0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
    SquareMatrix<int, 3> nilpotent{nilpotent_rows};
    const auto nilpotent2 = nth_power(nilpotent, 2);
    EXPECT_EQ(nilpotent2(0, 2), 1);
    EXPECT_EQ(nilpotent2(0, 1), 0);
    EXPECT_EQ(nth_power(nilpotent, 3), (SquareMatrix<int, 3>{}));
    EXPECT_EQ((nth_power(SquareMatrix<int, 3>::identity(), 9)),
              (SquareMatrix<int, 3>::identity()));
}


TEST(NthPowerMatrices, NegativePowers) {
    int shear_rows[2][2] = {{1, 1}, {0, 1}};
    SquareMatrix<int, 2> shear{shear_rows};
    int shear_inv_rows[2][2] = {{1, -1}, {0, 1}};
    int shear_inv4_rows[2][2] = {{1, -4}, {0, 1}};
    EXPECT_EQ(nth_power(shear, -1), (SquareMatrix<int, 2>{shear_inv_rows}));
    EXPECT_EQ(nth_power(shear, -4), (SquareMatrix<int, 2>{shear_inv4_rows}));
    EXPECT_EQ(nth_power(shear, -4) * nth_power(shear, 4),
              (SquareMatrix<int, 2>::identity()));
}

struct MulInt {
    int value = 1;

    static constexpr MulInt identity() { return MulInt{1}; }

    MulInt& operator*=(const MulInt& rhs) {
        value *= rhs.value;
        return *this;
    }

    friend bool operator==(MulInt lhs, MulInt rhs) {
        return lhs.value == rhs.value;
    }
};


TEST(NthPowerMatrices, CustomMatrixType) {
    EXPECT_EQ(nth_power(MulInt{3}, 4).value, 81);
    EXPECT_EQ(nth_power(MulInt{3}, 0).value, 1);
    EXPECT_EQ(nth_power(MulInt{-2}, 5).value, -32);
}



int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}