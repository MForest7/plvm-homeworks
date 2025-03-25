## Build and run

```
cmake -B build
cmake --build build
./build/mempool
```

```
With std::allocator:
Time used:       366594 usec
Memory used:     316227584 bytes
Memory required: 160000000 bytes 
Overhead: 49.4%
With PoolAllocator:
Time used:       11449 usec
Memory used:     160088064 bytes
Memory required: 160000000 bytes 
Overhead:  0.1%
```
