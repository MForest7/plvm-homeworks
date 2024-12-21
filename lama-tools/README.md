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

real    0m2.654s
user    0m2.642s
sys     0m0.012s

Original source code interpreter

real    0m6.167s
user    0m6.135s
sys     0m0.033s

Original bytecode interpreter

real    0m2.159s
user    0m2.136s
sys     0m0.024s
```