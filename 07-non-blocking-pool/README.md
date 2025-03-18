## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       17285722 usec
Memory used:     5122965504 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       27774070 usec
Memory used:     2563284992 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       24704908 usec
Memory used:     2563301376 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       1050368 usec
Memory used:     1532514304 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
