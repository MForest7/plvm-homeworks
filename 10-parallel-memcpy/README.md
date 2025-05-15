## Build and run

```
cmake -Bbuild
make -C build
./build/parallel-memcpy
```

## Результаты измерений

При запуске `memcpy` с произвольным количеством потоков время исполнения практически не изменяется.

```
0 threads: 23743.5 us
1 threads: 22883.9 us
2 threads: 24322.4 us
3 threads: 24250.2 us
4 threads: 24858.7 us
5 threads: 24181.3 us
6 threads: 24548.8 us
7 threads: 24747.8 us
8 threads: 24799.2 us
```

