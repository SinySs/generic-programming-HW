## Задача

Объяснить, что не так с таким кодом:

```cpp
template <typename... Ts>
concept Addable =
    requires(Ts... p) {
        (... + p);
        requires sizeof...(Ts) > 1;
    };

template <Addable... Ts>
auto sum_all(Ts... p) {
    return (... + p);
}

sum_all(1, 2); // FAIL
```

## Диагноз

Concept `Addable` принимает произвольное число типов (`Addable<T1, T2, ...>`)
и проверяет, что их можно сложить fold-ом и что типов больше одного.

Но запись

```cpp
template <Addable... Ts>
```

означает не `Addable<Ts...>`. По стандарту constrained parameter pack
раскрывается **поэлементно**:

```cpp
template <typename... Ts>
  requires (Addable<Ts> && ...)
auto sum_all(Ts... p);
```

Для вызова `sum_all(1, 2)` компилятор проверяет:

1. `Addable<int>`
2. `Addable<int>`

а не `Addable<int, int>`.

Внутри `Addable<int>`:

- `sizeof...(Ts) == 1`
- nested requirement `requires sizeof...(Ts) > 1` ложно
- значит `Addable<int> == false`
- значит `(Addable<Ts> && ...) == false`
- `sum_all` исключается из overload set -> ошибка компиляции

## Ссылки на стандарт

Типичный пример из [temp.param]:

```cpp
template<C T> struct s1;     // associates C<T>
template<C... T> struct s2;  // associates (C<T> && ...)
```

## Содержание директории

| Файл | Назначение |
|---|---|
| [`broken.cpp`](broken.cpp) | исходный пример из презентации - не компилируется |
| [`fixed.cpp`](fixed.cpp) | исправленная версия - компилируется и работает |

## Исправление

Нужно применить concept ко **всему** пакету одной concept-id:


```cpp
template <typename... Ts>
  requires Addable<Ts...>
auto sum_all(Ts... p) {
    return (... + p);
}
```

Теперь для `sum_all(1, 2)` проверяется `Addable<int, int>`:


- fold `(... + p)` валиден;
- `sizeof...(Ts) > 1` истинно.

## Как проверить

```bash
# ожидается ошибка компиляции
g++ -std=c++20 -c broken.cpp

# ожидается успешная сборка и вывод 3 и 6
g++ -std=c++20 fixed.cpp -o fixed && ./fixed
```
