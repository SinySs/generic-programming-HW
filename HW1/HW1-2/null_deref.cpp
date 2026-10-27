#include <concepts>
#include <type_traits>

//1
template<class T, class U>
decltype((*(T*)nullptr) + (*(U*)nullptr)) add(T, U);

static_assert(std::is_same_v<decltype(add(1, 2)), int>);
static_assert(std::is_same_v<decltype(*(int*)nullptr), int&>);

//2
template<class T>
concept NullPointeeIsLvalue = requires {
    { *static_cast<T*>(nullptr) } -> std::same_as<T&>;
};

template<NullPointeeIsLvalue T>
constexpr int pointee_size() {
    return sizeof(T);
}

int main() {
    static_assert(pointee_size<int>()    == sizeof(int));
    static_assert(pointee_size<double>() == sizeof(double));
    return 0;
}
