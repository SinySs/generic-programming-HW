# HW2.1 - `cow::cow_string`

## Сборка и запуск

```bash
cmake -S . -B build
cmake --build build -j
./build/cow_tests
./build/cow_bench
```

## Бенчмарк

`cow_bench` сравнивает `std::string` и `cow::string`. `std/cow` больше 1 - `cow::string` быстрее.

- **pass by value, read** - строку передают по значению и читают три символа. Длины 8, 64, 256, 4096 и 65536 байт. Короткие строки у обеих реализаций живут внутри объекта, длинные `cow::string` только увеличивает счётчик ссылок.
- **pass by value, mutate first, len=4096** - копию передают по значению и меняют первый символ. `cow::string` перед записью отцепляет общий буфер.
- **append 64 x 32-byte chunks** - пустую строку 64 раза дописывают куском по 32 байта.
- **copy vector of 1000 x 4KiB** - вектор из 1000 копий одной строки длиной 4 КиБ копируют целиком. У `std::string` копируются все буферы, у `cow::string` - указатели на один общий.

```
COW basic_string vs std::string
compiler: 14.3.0
sizeof std::string = 32, SSO limit = 15
sizeof cow::string = 40, SSO capacity = 15
Copies of a long cow::string share one buffer.
std/cow > 1 means cow::string is faster.

scenario                                           iters     std ns/op     cow ns/op   std/cow
pass by value, read, len=8                        400000           4.3           4.4      0.99
pass by value, read, len=64                       150000          12.0          13.0      0.92
pass by value, read, len=256                       80000          14.2          13.0      1.10
pass by value, read, len=4096                      20000          48.5          12.9      3.76
pass by value, read, len=65536                      3000        1320.1          13.3     98.91
pass by value, mutate first, len=4096               8000          48.4          76.6      0.63
append 64 x 32-byte chunks                          8000         203.2         481.3      0.42
copy vector of 1000 x 4KiB                            30     1166236.0       13740.9     84.87
sink=12618388498127998944
```
