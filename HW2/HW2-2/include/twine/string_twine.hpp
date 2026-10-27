#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace twine {

template <typename T>
concept dangling_owner =
    std::is_rvalue_reference_v<T&&> && requires(const std::remove_cvref_t<T>& value) { value.c_str(); };

template <typename T, typename CharT, typename Traits>
concept owned_view = !dangling_owner<T> && !std::is_null_pointer_v<std::remove_cvref_t<T>> &&
                     std::convertible_to<T, std::basic_string_view<CharT, Traits>>;

template <typename CharT, typename Traits = std::char_traits<CharT>>
class string_twine {
public:
    using view = std::basic_string_view<CharT, Traits>;
    using string = std::basic_string<CharT, Traits>;

    string_twine() = default;

    template <typename First, typename... Rest>
    requires((owned_view<First, CharT, Traits> || std::is_same_v<std::remove_cvref_t<First>, string_twine>) &&
             ((owned_view<Rest, CharT, Traits> || std::is_same_v<std::remove_cvref_t<Rest>, string_twine>) &&
              ...) &&
             (sizeof...(Rest) != 0 || !std::is_same_v<std::remove_cvref_t<First>, string_twine>))
        string_twine(First&& first, Rest&&... rest) {
        std::vector<std::shared_ptr<const Node>> nodes;
        nodes.reserve(1 + sizeof...(Rest));
        push_piece(nodes, std::forward<First>(first));
        (push_piece(nodes, std::forward<Rest>(rest)), ...);
        root_ = combine(std::move(nodes));
    }

    string_twine(string&&) = delete;
    string_twine(std::nullptr_t) = delete;

    string str() const {
        string result;
        if (root_ == nullptr) {
            return result;
        }
        if (root_->size > result.max_size()) {
            throw std::length_error("string_twine::str");
        }
        result.reserve(root_->size);
        append(result, root_.get());
        return result;
    }

    std::size_t size() const noexcept { return root_ == nullptr ? 0 : root_->size; }
    std::size_t leaf_count() const noexcept { return root_ == nullptr ? 0 : root_->leaves; }
    int height() const noexcept { return root_ == nullptr ? 0 : root_->height; }
    bool empty() const noexcept { return size() == 0; }

    bool balanced() const { return is_balanced(root_.get()); }

    string_twine operator+(const string_twine& rhs) const {
        string_twine out;
        out.root_ = join(root_, rhs.root_);
        return out;
    }

    string_twine& operator+=(const string_twine& rhs) {
        root_ = join(root_, rhs.root_);
        return *this;
    }

private:
    struct Node {
        view piece{};
        std::shared_ptr<const Node> left;
        std::shared_ptr<const Node> right;
        std::size_t size = 0;
        std::size_t leaves = 0;
        int height = 0;
        bool is_leaf = false;
    };

    std::shared_ptr<const Node> root_;

    static int height_of(const Node* node) noexcept { return node == nullptr ? 0 : node->height; }

    static std::shared_ptr<const Node> make_leaf(view piece) {
        if (piece.empty()) {
            return nullptr;
        }
        auto node = std::make_shared<Node>();
        node->piece = piece;
        node->size = piece.size();
        node->leaves = 1;
        node->height = 1;
        node->is_leaf = true;
        return node;
    }

    static std::shared_ptr<const Node> make_internal(std::shared_ptr<const Node> left,
                                                     std::shared_ptr<const Node> right) {
        assert(left != nullptr && right != nullptr);
        if (left->size > std::numeric_limits<std::size_t>::max() - right->size ||
            left->leaves > std::numeric_limits<std::size_t>::max() - right->leaves) {
            throw std::length_error("string_twine");
        }
        auto node = std::make_shared<Node>();
        node->size = left->size + right->size;
        node->leaves = left->leaves + right->leaves;
        node->height = std::max(left->height, right->height) + 1;
        node->is_leaf = false;
        node->left = std::move(left);
        node->right = std::move(right);
        return node;
    }

    static std::shared_ptr<const Node> rotate_left(const Node& node) {
        const std::shared_ptr<const Node>& right = node.right;
        assert(right != nullptr && right->left != nullptr && right->right != nullptr);
        auto new_left = make_internal(node.left, right->left);
        return make_internal(std::move(new_left), right->right);
    }

    static std::shared_ptr<const Node> rotate_right(const Node& node) {
        const std::shared_ptr<const Node>& left = node.left;
        assert(left != nullptr && left->left != nullptr && left->right != nullptr);
        auto new_right = make_internal(left->right, node.right);
        return make_internal(left->left, std::move(new_right));
    }

    static std::shared_ptr<const Node> rebalance(std::shared_ptr<const Node> node) {
        const int balance = height_of(node->left.get()) - height_of(node->right.get());
        if (balance > 1) {
            if (height_of(node->left->left.get()) < height_of(node->left->right.get())) {
                auto new_left = rotate_left(*node->left);
                node = make_internal(std::move(new_left), node->right);
            }
            return rotate_right(*node);
        }
        if (balance < -1) {
            if (height_of(node->right->right.get()) < height_of(node->right->left.get())) {
                auto new_right = rotate_right(*node->right);
                node = make_internal(node->left, std::move(new_right));
            }
            return rotate_left(*node);
        }
        return node;
    }

    static std::shared_ptr<const Node> join(std::shared_ptr<const Node> left, std::shared_ptr<const Node> right) {
        if (left == nullptr) {
            return right;
        }
        if (right == nullptr) {
            return left;
        }
        if (left->height > right->height + 1) {
            auto new_right = join(left->right, std::move(right));
            return rebalance(make_internal(left->left, std::move(new_right)));
        }
        if (right->height > left->height + 1) {
            auto new_left = join(std::move(left), right->left);
            return rebalance(make_internal(std::move(new_left), right->right));
        }
        return make_internal(std::move(left), std::move(right));
    }

    static std::shared_ptr<const Node> combine(std::vector<std::shared_ptr<const Node>> level) {
        if (level.empty()) {
            return nullptr;
        }
        while (level.size() > 1) {
            std::vector<std::shared_ptr<const Node>> next;
            next.reserve(level.size() / 2 + 1);
            std::size_t index = 0;
            for (; index + 1 < level.size(); index += 2) {
                next.push_back(join(std::move(level[index]), std::move(level[index + 1])));
            }
            if (index < level.size()) {
                next.push_back(std::move(level[index]));
            }
            level = std::move(next);
        }
        return std::move(level.front());
    }

    template <typename T>
    static std::shared_ptr<const Node> as_node(T&& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, string_twine>) {
            return std::forward<T>(value).root_;
        } else if constexpr (std::is_null_pointer_v<U>) {
            return nullptr;
        } else if constexpr (std::is_pointer_v<U>) {
            if (value == nullptr) {
                return nullptr;
            }
            return make_leaf(view(value));
        } else {
            return make_leaf(view(std::forward<T>(value)));
        }
    }

    template <typename T>
    static void push_piece(std::vector<std::shared_ptr<const Node>>& nodes, T&& value) {
        if (auto node = as_node(std::forward<T>(value))) {
            nodes.push_back(std::move(node));
        }
    }

    static void append(string& out, const Node* node) {
        if (node->is_leaf) {
            out.append(node->piece.data(), node->piece.size());
            return;
        }
        append(out, node->left.get());
        append(out, node->right.get());
    }

    static bool is_balanced(const Node* node) {
        if (node == nullptr) {
            return true;
        }
        if (node->is_leaf) {
            return node->left == nullptr && node->right == nullptr && node->leaves == 1 && node->size == node->piece.size() &&
                   !node->piece.empty();
        }
        if (node->left == nullptr || node->right == nullptr) {
            return false;
        }
        const int diff = node->left->height - node->right->height;
        return diff <= 1 && diff >= -1 && node->size == node->left->size + node->right->size &&
               node->leaves == node->left->leaves + node->right->leaves && is_balanced(node->left.get()) &&
               is_balanced(node->right.get());
    }
};

template <typename CharT, typename Traits, typename... Rest>
string_twine(std::basic_string_view<CharT, Traits>, Rest&&...) -> string_twine<CharT, Traits>;

template <typename CharT, typename... Rest>
string_twine(const CharT*, Rest&&...) -> string_twine<CharT>;

template <typename CharT, typename Traits, typename Allocator, typename... Rest>
string_twine(const std::basic_string<CharT, Traits, Allocator>&, Rest&&...) -> string_twine<CharT, Traits>;

template <typename CharT, typename Traits, typename... Rest>
string_twine(const string_twine<CharT, Traits>&, Rest&&...) -> string_twine<CharT, Traits>;

template <typename CharT, typename Traits, typename T>
requires owned_view<T, CharT, Traits>
    string_twine<CharT, Traits> operator+(T&& lhs, const string_twine<CharT, Traits>& rhs) {
    return string_twine<CharT, Traits>(std::forward<T>(lhs)) + rhs;
}

}
