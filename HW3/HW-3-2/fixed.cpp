#include <iostream>

template <typename... Ts>
concept Addable = requires(Ts... p) {
    (... + p);
    requires sizeof...(Ts) > 1;
};

template <typename... Ts>
    requires Addable<Ts...>
auto sum_all(Ts... p) {
    return (... + p);
}

int main() {
    std::cout << sum_all(1, 2) << '\n';      // 3
    std::cout << sum_all(1, 2, 3) << '\n';   // 6
    // sum_all(1);  // error: sizeof...(Ts) > 1 
}
