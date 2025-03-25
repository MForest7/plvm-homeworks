## Build and run

```
cmake -Bbuild
make -C build
./build/non-blocking-pool <allocator>
```

## Результаты измерений

### std::allocator
```
Time used:       15387236 usec
Memory used:     5123633152 bytes
Memory required: 2560000000 bytes 
Overhead: 50.0%
```

### Глобальный пул под мьютексом
```
Time used:       28345397 usec
Memory used:     2563362816 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Глобальный неблокирующий пул
```
Time used:       25214296 usec
Memory used:     2563371008 bytes
Memory required: 2560000000 bytes 
Overhead:  0.1%
```

### Локальные в потоках пулы
```
Time used:       944684 usec
Memory used:     1509416960 bytes
Memory required: 2560000000 bytes 
Overhead:  0.0%
```
