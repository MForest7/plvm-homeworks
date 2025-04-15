## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       14343302 usec
Memory used:     5123543040 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       20395554 usec
Memory used:     2563317760 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       23539764 usec
Memory used:     2563260416 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       1247280 usec
Memory used:     1877663744 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
