## Build and run

```
cmake -Bbuild
make -C build
./build/mempool
```

```
With std::allocator:
Time used:       356681 usec
Memory used:     316059648 bytes
Memory required: 160000000 bytes 
Overhead: 49.4%
With PoolAllocator:
Time used:       16772 usec
Memory used:     160088064 bytes
Memory required: 160000000 bytes 
Overhead:  0.1%
```
