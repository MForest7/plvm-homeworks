## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       15714803 usec
Memory used:     5123375104 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       28099937 usec
Memory used:     2563325952 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       18975348 usec
Memory used:     2563366912 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       1061161 usec
Memory used:     1463791616 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
