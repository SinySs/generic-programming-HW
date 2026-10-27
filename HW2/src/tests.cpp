#include "cow/cow_string.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

TEST(CowString, EmptyAndShortCopyAreIndependent) {
    cow::string empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.c_str()[0], '\0');
    EXPECT_TRUE(empty.unique());
    EXPECT_EQ(empty.capacity(), cow::string::sso_capacity);

    cow::string hello("hello");
    EXPECT_EQ(hello, "hello");
    EXPECT_TRUE(hello.unique());

    cow::string copy = hello;
    EXPECT_EQ(copy, hello);
    EXPECT_TRUE(hello.unique());
    EXPECT_TRUE(copy.unique());
    copy[0] = 'H';
    EXPECT_EQ(hello, "hello");
    EXPECT_EQ(copy, "Hello");
}

TEST(CowString, LongCopySharesUntilWrite) {
    cow::string original(64, 'a');
    EXPECT_GE(original.capacity(), 64u);
    EXPECT_TRUE(original.unique());

    cow::string shared = original;
    EXPECT_EQ(original.use_count(), 2u);
    EXPECT_EQ(shared.use_count(), 2u);
    EXPECT_EQ(std::as_const(shared)[63], 'a');

    shared[0] = 'b';
    EXPECT_TRUE(shared.unique());
    EXPECT_TRUE(original.unique());
    EXPECT_EQ(std::as_const(original)[0], 'a');
    EXPECT_EQ(std::as_const(shared)[0], 'b');
}

TEST(CowString, ManyCopiesShareOneBuffer) {
    cow::string original(1024, 'x');
    std::vector<cow::string> copies;
    copies.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        copies.push_back(original);
    }
    EXPECT_EQ(original.use_count(), 1001u);

    copies[10][0] = 'y';
    EXPECT_EQ(original.use_count(), 1000u);
    EXPECT_TRUE(copies[10].unique());
    EXPECT_EQ(std::as_const(copies[10])[0], 'y');
    EXPECT_EQ(std::as_const(original)[0], 'x');
    EXPECT_EQ(std::as_const(copies[11])[0], 'x');
}

TEST(CowString, MutableAliasIsNotShared) {
    cow::string short_s("str");
    const char* short_p = short_s.data();
    {
        cow::string other(short_s);
        short_s[0] = 'S';
        EXPECT_EQ(other, "str");
    }
    EXPECT_EQ(*short_p, 'S');
    EXPECT_EQ(short_s, "Str");

    cow::string long_s(64, 's');
    const char* pointer = long_s.data();
    {
        cow::string other(long_s);
        EXPECT_TRUE(long_s.unique());
        EXPECT_TRUE(other.unique());
        long_s[0] = 'S';
        EXPECT_EQ(std::as_const(other)[0], 's');
    }
    EXPECT_EQ(*pointer, 'S');
    EXPECT_EQ(std::as_const(long_s)[0], 'S');
}

TEST(CowString, ConstDataKeepsTheOtherOwnerAlive) {
    cow::string value(64, 'a');
    const char* pointer = std::as_const(value).data();
    cow::string other = value;
    EXPECT_EQ(value.use_count(), 2u);

    value[0] = 'b';
    EXPECT_TRUE(value.unique());
    EXPECT_TRUE(other.unique());
    EXPECT_EQ(std::as_const(value)[0], 'b');
    EXPECT_EQ(std::as_const(other)[0], 'a');
    EXPECT_EQ(pointer[0], 'a');
    EXPECT_EQ(pointer[63], 'a');
}

TEST(CowString, AppendDetachesAndAcceptsSelf) {
    cow::string value(64, 'a');
    cow::string other = value;
    value.append("xyz", 3);
    EXPECT_EQ(value.size(), 67u);
    EXPECT_EQ(value[64], 'x');
    EXPECT_EQ(other.size(), 64u);
    EXPECT_EQ(std::as_const(other)[0], 'a');
    EXPECT_TRUE(value.unique());
    EXPECT_TRUE(other.unique());

    cow::string doubled("hello");
    doubled.append(doubled);
    EXPECT_EQ(doubled, "hellohello");
}

TEST(CowString, MoveAndAssign) {
    cow::string source(64, 'm');
    cow::string moved = std::move(source);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(moved.size(), 64u);
    EXPECT_TRUE(moved.unique());
    EXPECT_EQ(std::as_const(moved)[0], 'm');
    source = "x";
    EXPECT_EQ(source, "x");
    EXPECT_EQ(moved.size(), 64u);

    cow::string shared(64, 'm');
    cow::string alias = shared;
    shared = std::move(alias);
    EXPECT_TRUE(alias.empty());
    EXPECT_EQ(shared.size(), 64u);
    EXPECT_TRUE(shared.unique());

    cow::string assigned(64, 'z');
    assigned = shared;
    EXPECT_EQ(assigned.use_count(), 2u);
    EXPECT_EQ(assigned, shared);
    assigned = "hi";
    EXPECT_EQ(assigned, "hi");
    EXPECT_EQ(shared.size(), 64u);
    EXPECT_TRUE(shared.unique());

    cow::string self(32, 'a');
    self = self;
    EXPECT_EQ(self.size(), 32u);
    self = self.c_str();
    EXPECT_EQ(self.size(), 32u);

    const char raw[] = {'a', '\0', 'b'};
    cow::string embedded(raw, 3);
    EXPECT_EQ(embedded.size(), 3u);
    EXPECT_EQ(embedded[1], '\0');
    EXPECT_EQ(embedded[2], 'b');
    EXPECT_LT(cow::string("abc"), cow::string("abd"));
    EXPECT_EQ(cow::string("abc"), "abc");
}

TEST(CowString, AssignOfSharedBufferKeepsTheCount) {
    cow::string first(64, 'q');
    cow::string second = first;
    cow::string third = first;
    second = third;
    EXPECT_EQ(first.use_count(), 3u);
    EXPECT_EQ(second.use_count(), 3u);
    EXPECT_EQ(third.use_count(), 3u);
    EXPECT_EQ(second, first);
    EXPECT_EQ(std::as_const(first)[0], 'q');
}

TEST(CowString, AssignOverMutableAliasKeepsThePointer) {
    cow::string value(64, 'a');
    char* pointer = value.data();
    cow::string incoming(64, 'b');
    value = incoming;
    EXPECT_EQ(pointer[0], 'b');
    EXPECT_EQ(pointer[63], 'b');
    EXPECT_TRUE(value.unique());
    EXPECT_TRUE(incoming.unique());
    EXPECT_EQ(value, incoming);

    cow::string copy = value;
    EXPECT_TRUE(value.unique());
    EXPECT_TRUE(copy.unique());
    copy = "x";
    EXPECT_EQ(pointer[0], 'b');
    EXPECT_EQ(pointer[63], 'b');
    EXPECT_EQ(value, incoming);
}

TEST(CowString, ClearDropsSharedBufferAndKeepsUniqueCapacity) {
    cow::string shared(64, 'c');
    cow::string other = shared;
    const std::size_t cap = shared.capacity();
    shared.clear();
    EXPECT_TRUE(shared.empty());
    EXPECT_EQ(shared.c_str()[0], '\0');
    EXPECT_EQ(shared.capacity(), cow::string::sso_capacity);
    EXPECT_TRUE(other.unique());
    EXPECT_EQ(other.size(), 64u);
    EXPECT_EQ(std::as_const(other)[0], 'c');

    other.clear();
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.capacity(), cap);
    other.append("z", 1);
    EXPECT_EQ(other, "z");

    cow::string pinned(64, 'p');
    char* pointer = pinned.data();
    pinned.clear();
    cow::string copy = pinned;
    EXPECT_TRUE(pinned.empty());
    EXPECT_TRUE(copy.empty());
    EXPECT_TRUE(pinned.unique());
    EXPECT_TRUE(copy.unique());
    pinned.append("hello");
    EXPECT_STREQ(pointer, "hello");
    copy = "x";
    EXPECT_STREQ(pointer, "hello");
    EXPECT_EQ(pinned, "hello");
}

TEST(CowString, SwapExchangesStorage) {
    cow::string shared(64, 'L');
    cow::string alias = shared;
    cow::string small("s");
    shared.swap(small);
    EXPECT_EQ(shared, "s");
    EXPECT_TRUE(shared.unique());
    EXPECT_EQ(small.size(), 64u);
    EXPECT_EQ(small.use_count(), 2u);
    EXPECT_EQ(alias.use_count(), 2u);
    EXPECT_EQ(alias, small);

    cow::string left("ab");
    cow::string right("vwxyz");
    swap(left, right);
    EXPECT_EQ(left, "vwxyz");
    EXPECT_EQ(right, "ab");

    small.swap(small);
    EXPECT_EQ(small.use_count(), 2u);
    EXPECT_EQ(small.size(), 64u);
}

TEST(CowString, PlusConcatenatesWithoutDetachingSources) {
    cow::string hello("hello");
    cow::string world("world");
    EXPECT_EQ(hello + world, "helloworld");
    EXPECT_EQ(hello + "!", "hello!");
    EXPECT_EQ("!" + world, "!world");
    EXPECT_EQ(hello + '!', "hello!");
    EXPECT_EQ('!' + world, "!world");
    EXPECT_EQ(hello + hello, "hellohello");
    EXPECT_EQ(hello, "hello");

    cow::string moved = std::move(hello) + world;
    EXPECT_EQ(moved, "helloworld");
    EXPECT_TRUE(hello.empty());

    cow::string original(64, 'a');
    cow::string alias = original;
    cow::string sum = original + cow::string("!");
    EXPECT_EQ(original.use_count(), 2u);
    EXPECT_EQ(alias.use_count(), 2u);
    EXPECT_EQ(sum.size(), 65u);
    EXPECT_EQ(sum[0], 'a');
    EXPECT_EQ(sum[64], '!');
}
