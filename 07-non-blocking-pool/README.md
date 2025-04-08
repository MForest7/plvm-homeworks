## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       19657500 usec
Memory used:     5123436544 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       49676150 usec
Memory used:     2563346432 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       47138368 usec
Memory used:     2563317760 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       1383748 usec
Memory used:     1493196800 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
