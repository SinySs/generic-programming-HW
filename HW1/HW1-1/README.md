# HW1.1. `volatile std::nullptr_t`

```cpp
#include <cstddef>

int f() {
    volatile std::nullptr_t a = nullptr;
    int* b;
    b = a;
    return b == nullptr;
}
```

## Что генерируют

Clang:

```asm
f():
        movq    $0, -8(%rsp)     # записать инициализатор a
        movl    $1, %eax         # b == nullptr, без чтения a
        retq
```

GCC:

```asm
f():
        movq    $0, -8(%rsp)     # записать инициализатор a
        movq    -8(%rsp), %rax   # прочитать volatile a
        movl    $1, %eax         # прочитанное значение не используется
        ret
```

## Почему чтение кажется обязательным

`a` - volatile-glvalue, а `b = a` берёт его значение: правый операнд приводится к типу левого через lvalue-to-rvalue conversion. Для обычного скаляра это преобразование объект читает. Чтение volatile-glvalue - побочный эффект, и его нельзя ни выкинуть, ни заменить заранее известным значением: обращения к volatile выполняются строго по абстрактной машине ([intro.abstract]).

Для `volatile int` перезагрузка ячейки была бы обязательна. GCC делает то же с `std::nullptr_t`: после записи инициализатора идёт `movq -8(%rsp), %rax`.

## Почему здесь читать не нужно

До общего правила «прочитать объект» в [conv.lval] стоит отдельный случай. Преобразование `std::nullptr_t` к объекту не обращается:

> If T is cv std::nullptr_t, the result is a null pointer constant ([conv.ptr]).
>
> [Note: Since the conversion does not access the object to which the glvalue refers, there is no side effect even if T is volatile-qualified ([intro.execution]), and the glvalue can refer to an inactive member of a union ([class.union]). - end note]
>

Результат и так известен: null pointer constant, дальше нулевой `int*`. В абстрактной машине есть только запись инициализатора. Чтения нет, значит и наблюдаемого volatile-доступа нет, и дописывать его нельзя.

Clang так и генерирует код: `movq $0, -8(%rsp)` и сразу `1`. Это соответствует стандарту. Чтение у GCC - лишнее.
