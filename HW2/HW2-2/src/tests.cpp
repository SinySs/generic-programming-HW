#include "twine/string_twine.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using twine::string_twine;

TEST(StringTwine, ExampleFromAssignment) {
    std::string_view sv = "Hello,", sv2 = "World!";
    const auto s = string_twine(sv, " ", sv2).str();
    EXPECT_EQ(s, "Hello, World!");
    EXPECT_EQ(s.c_str()[s.size()], '\0');
}

TEST(StringTwine, EmptyPiecesAreSkipped) {
    string_twine<char> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.leaf_count(), 0u);
    EXPECT_EQ(empty.height(), 0);
    EXPECT_TRUE(empty.balanced());
    EXPECT_EQ(empty.str(), "");

    const std::string_view word = "ab";
    const string_twine skipped("", word, "", "c", std::string_view{});
    EXPECT_EQ(skipped.str(), "abc");
    EXPECT_EQ(skipped.leaf_count(), 2u);
    EXPECT_TRUE(skipped.balanced());
}

TEST(StringTwine, SingleLeafAndEmbeddedNull) {
    constexpr char raw[] = {'a', '\0', 'b'};
    const std::string_view piece(raw, 3);
    const string_twine one(piece);
    EXPECT_EQ(one.height(), 1);
    EXPECT_EQ(one.leaf_count(), 1u);
    EXPECT_EQ(one.size(), 3u);
    EXPECT_EQ(one.str(), std::string(raw, 3));

    const std::string_view tail("c", 1);
    EXPECT_EQ((one + tail).str(), std::string("a\0bc", 4));
}

TEST(StringTwine, LiteralsPointersAndLvalueString) {
    const std::string owned = "owned";
    const char* pointer = "ptr";
    const std::string_view view = "view";
    const auto text = string_twine(pointer, "-", owned, "-", view, "!").str();
    EXPECT_EQ(text, "ptr-owned-view!");

    const char* missing = nullptr;
    EXPECT_EQ(string_twine(missing, "ok").str(), "ok");
}

TEST(StringTwine, TemporaryStringIsRejected) {
    static_assert(!std::is_constructible_v<string_twine<char>, std::string>);
    static_assert(!std::is_constructible_v<string_twine<char>, std::string&&>);
    static_assert(std::is_constructible_v<string_twine<char>, const std::string&>);
    static_assert(std::is_constructible_v<string_twine<char>, std::string_view>);
    static_assert(std::is_constructible_v<string_twine<char>, const char*>);
    static_assert(!std::is_constructible_v<string_twine<char>, std::nullptr_t>);
}

TEST(StringTwine, CopySharesMoveLeavesSourceEmpty) {
    std::string buffer = "share";
    string_twine original(std::string_view{buffer});
    const string_twine copy = original;
    EXPECT_EQ(copy.str(), "share");
    EXPECT_EQ(original.leaf_count(), copy.leaf_count());

    buffer[0] = 'S';
    EXPECT_EQ(original.str(), "Share");
    EXPECT_EQ(copy.str(), "Share");

    string_twine moved = std::move(original);
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(moved.str(), "Share");
}

TEST(StringTwine, ConcatenationOrderAndSelfConcat) {
    const string_twine a("a");
    const string_twine b("b");
    const string_twine c("c");
    EXPECT_EQ(((a + b) + c).str(), "abc");
    EXPECT_EQ((a + (b + c)).str(), "abc");
    EXPECT_EQ(("[" + a + "]").str(), "[a]");

    string_twine doubled("ab");
    doubled += doubled;
    EXPECT_EQ(doubled.str(), "abab");
    EXPECT_EQ(doubled.leaf_count(), 2u);
    EXPECT_TRUE(doubled.balanced());
}

TEST(StringTwine, LeftFoldStaysLogarithmic) {
    constexpr int count = 1024;
    string_twine<char> folded("x");
    for (int i = 1; i < count; ++i) {
        folded += "x";
        EXPECT_TRUE(folded.balanced());
    }
    EXPECT_EQ(folded.size(), static_cast<std::size_t>(count));
    EXPECT_EQ(folded.leaf_count(), static_cast<std::size_t>(count));
    EXPECT_EQ(folded.str(), std::string(static_cast<std::size_t>(count), 'x'));
    // 1024 leaves need height at least 10. An AVL tree stays well under 2 log2(N).
    EXPECT_GE(folded.height(), 10);
    EXPECT_LE(folded.height(), 16);
}

TEST(StringTwine, WideCharacters) {
    const std::wstring_view left = L"ab";
    const std::wstring_view right = L"cd";
    const auto text = string_twine(left, L" ", right).str();
    EXPECT_EQ(text, std::wstring(L"ab cd"));
}