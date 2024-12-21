# Lama bytecode interpreter

## Build

```
make
```

## Run tests

Regression tests
```
make regression
```

Generated expressions:
```
make regression-expressions
```

Performance:
```
make performance
```

Perfomance test output:
```
Test: Sort

Bytecode interpreter
Verification time: 0.085119ms
Interpretation time: 2587.76ms

real    0m2.590s
user    0m2.578s
sys     0m0.012s

Original source code interpreter

real    0m5.992s
user    0m5.964s
sys     0m0.029s

Original bytecode interpreter

real    0m2.265s
user    0m2.237s
sys     0m0.028s
```