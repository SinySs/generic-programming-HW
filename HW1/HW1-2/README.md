# HW1.2. Ещё одно корректное разыменование нулевого указателя (1)
Из https://eel.is/c++draft/dcl.fct

```cpp
template<class T, class U>
decltype((*(T*)0) + (*(U*)0)) add(T t, U u);
```

Тот же приём в проверяемом виде - `HW1-2/null_deref.cpp`:

```cpp
template<class T, class U>
decltype((*(T*)nullptr) + (*(U*)nullptr)) add(T, U);

static_assert(std::is_same_v<decltype(add(1, 2)), int>);
static_assert(std::is_same_v<decltype(*(int*)nullptr), int&>);
```

## Ещё один пример (2)

```cpp
template<class T>
concept NullPointeeIsLvalue = requires {
    { *static_cast<T*>(nullptr) } -> std::same_as<T&>;
};

template<NullPointeeIsLvalue T>
constexpr int pointee_size() {
    return sizeof(T);
}
```

Выражения внутри тела требования не вычисляются ([expr.prim.req.general](https://timsong-cpp.github.io/cppwp/n4950/expr.prim.req.general)):

> Expressions appearing within a requirement-body are unevaluated operands.

Составное требование смотрит на тип выражения так, как это сделал бы `decltype((E))`. `*static_cast<int*>(nullptr)` - lvalue типа `int`, концепт для `int` выполнен, и `pointee_size<int>()` возвращает `sizeof(int)`.



Для прверки: `g++ -std=c++23 -Wall -Wextra -Wpedantic -O2 null_deref.cpp`.