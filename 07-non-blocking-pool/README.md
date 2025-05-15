## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       13824034 usec
Memory used:     5123928064 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       29159532 usec
Memory used:     2563338240 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       26411371 usec
Memory used:     2563239936 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       1258747 usec
Memory used:     1503387648 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
