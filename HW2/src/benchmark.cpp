#include "cow/cow_string.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

volatile std::size_t g_sink = 0;

template <typename String>
[[gnu::noinline]] std::size_t read_by_value(String value) {
    if (value.empty()) {
        return 0;
    }
    const String& stable = value;
    std::size_t hash = stable.size();
    hash = hash * 131u + static_cast<unsigned char>(stable.data()[0]);
    hash = hash * 131u + static_cast<unsigned char>(stable.data()[stable.size() / 2]);
    hash = hash * 131u + static_cast<unsigned char>(stable.data()[stable.size() - 1]);
    return hash;
}

template <typename String>
[[gnu::noinline]] std::size_t mutate_by_value(String value) {
    value[0] = 'Z';
    const String& stable = value;
    return static_cast<unsigned char>(stable.data()[0]);
}

template <typename String>
[[gnu::noinline]] std::size_t build_by_append(const char* chunk, std::size_t chunk_size, std::size_t pieces) {
    String value;
    for (std::size_t i = 0; i < pieces; ++i) {
        value.append(chunk, chunk_size);
    }
    const String& stable = value;
    return stable.size() + static_cast<unsigned char>(stable.data()[0]);
}

template <typename String>
[[gnu::noinline]] std::size_t checksum_copies(std::vector<String> values) {
    std::size_t hash = values.size();
    for (const String& value : values) {
        hash = hash * 131u + static_cast<unsigned char>(value.data()[0]);
        hash = hash * 131u + static_cast<unsigned char>(value.data()[value.size() - 1]);
    }
    return hash;
}

template <typename Function>
double nanoseconds(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(stop - start).count();
}

std::size_t std_sso_limit() {
    std::size_t limit = 0;
    for (std::size_t length = 0; length <= 64; ++length) {
        std::string value(length, 'a');
        const auto data = reinterpret_cast<std::uintptr_t>(value.data());
        const auto object = reinterpret_cast<std::uintptr_t>(&value);
        const bool inside = data >= object && data < object + sizeof(value);
        if (!inside) {
            break;
        }
        limit = length;
    }
    return limit;
}

void print_row(const char* name, std::size_t iterations, double std_per_op, double cow_per_op) {
    const double ratio = cow_per_op == 0.0 ? 0.0 : std_per_op / cow_per_op;
    std::cout << std::left << std::setw(46) << name << std::right << std::setw(10) << iterations << std::setw(14)
              << std::fixed << std::setprecision(1) << std_per_op << std::setw(14) << cow_per_op << std::setw(10)
              << std::setprecision(2) << ratio << '\n';
}

std::string make_payload(std::size_t length) {
    std::string payload(length, 'x');
    if (length > 2) {
        payload.front() = 'A';
        payload[length / 2] = 'M';
        payload.back() = 'Z';
    }
    return payload;
}

int main() {
    std::cout << "COW basic_string vs std::string\n";
    std::cout << "compiler: " << __VERSION__ << '\n';
    std::cout << "sizeof std::string = " << sizeof(std::string) << ", SSO limit = " << std_sso_limit() << '\n';
    std::cout << "sizeof cow::string = " << sizeof(cow::string) << ", SSO capacity = " << cow::string::sso_capacity
              << '\n';
    std::cout << "Copies of a long cow::string share one buffer.\n";
    std::cout << "std/cow > 1 means cow::string is faster.\n\n";
    std::cout << std::left << std::setw(46) << "scenario" << std::right << std::setw(10) << "iters" << std::setw(14)
              << "std ns/op" << std::setw(14) << "cow ns/op" << std::setw(10) << "std/cow" << '\n';

    const char chunk[] = "abcdefghijklmnopqrstuvwxyz012345";

    for (const std::size_t length : {std::size_t{8}, std::size_t{64}, std::size_t{256}, std::size_t{4096}, std::size_t{65536}}) {
        const std::string payload = make_payload(length);
        const std::size_t iterations = length <= 8 ? 400000 : length <= 64 ? 150000 : length <= 256 ? 80000 : length <= 4096 ? 20000 : 3000;
        const std::string label = "pass by value, read, len=" + std::to_string(length);
        const double std_per_op = nanoseconds([&] {
            std::string source(payload);
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += read_by_value(source);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        const double cow_per_op = nanoseconds([&] {
            cow::string source(payload.data(), payload.size());
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += read_by_value(source);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        print_row(label.c_str(), iterations, std_per_op, cow_per_op);
    }

    {
        constexpr std::size_t iterations = 8000;
        const std::string payload = make_payload(4096);
        const double std_per_op = nanoseconds([&] {
            std::string source(payload);
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += mutate_by_value(source);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        const double cow_per_op = nanoseconds([&] {
            cow::string source(payload.data(), payload.size());
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += mutate_by_value(source);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        print_row("pass by value, mutate first, len=4096", iterations, std_per_op, cow_per_op);
    }

    {
        constexpr std::size_t pieces = 64;
        constexpr std::size_t iterations = 8000;
        const double std_per_op = nanoseconds([&] {
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += build_by_append<std::string>(chunk, sizeof(chunk) - 1, pieces);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        const double cow_per_op = nanoseconds([&] {
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += build_by_append<cow::string>(chunk, sizeof(chunk) - 1, pieces);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        print_row("append 64 x 32-byte chunks", iterations, std_per_op, cow_per_op);
    }

    {
        constexpr std::size_t copies = 1000;
        constexpr std::size_t length = 4096;
        constexpr std::size_t iterations = 30;
        const std::string payload = make_payload(length);
        std::vector<std::string> std_values;
        std::vector<cow::string> cow_values;
        std_values.reserve(copies);
        cow_values.reserve(copies);
        std::string std_source(payload);
        cow::string cow_source(payload.data(), payload.size());
        for (std::size_t i = 0; i < copies; ++i) {
            std_values.push_back(std_source);
            cow_values.push_back(cow_source);
        }
        const double std_per_op = nanoseconds([&] {
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += checksum_copies(std_values);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        const double cow_per_op = nanoseconds([&] {
            std::size_t hash = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                hash += checksum_copies(cow_values);
            }
            g_sink = hash;
        }) / static_cast<double>(iterations);
        print_row("copy vector of 1000 x 4KiB", iterations, std_per_op, cow_per_op);
    }

    std::cout << "sink=" << g_sink << '\n';
    return 0;
}
