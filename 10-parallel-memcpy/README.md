## Build and run

```
cmake -Bbuild
make -C build
./build/parallel-memcpy
```

## Результаты измерений

При запуске `memcpy` с произвольным количеством потоков время исполнения практически не изменяется.

```
0 threads: 24648.5 us
1 threads: 23791 us
2 threads: 26127.5 us
3 threads: 25018.3 us
4 threads: 26350.1 us
5 threads: 26906.5 us
6 threads: 25596.5 us
7 threads: 25841.3 us
8 threads: 26569.3 us
```

