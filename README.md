# Homework cpp multithreads Bubnov

## HW-1

```bash
```

## HW-2

```bash
cmake -S hw-2 -B hw-2/build && cmake --build hw-2/build
./hw-2/build/apply_function_tests
./hw-2/build/apply_function_benchmark
```

## HW-3

```bash
cmake -S hw-3 -B hw-3/build && cmake --build hw-3/build
./hw-3/build/buffered_channel_test
./hw-3/build/buffered_channel_benchmark

./hw-3/build/buffered_channel_benchmark --benchmark_format=json --benchmark_out=./hw-3/benchmark.json
python3 ./hw-3/check_benchmark.py ./hw-3/benchmark.json
```

## HW-4

```bash
cmake -S hw-4 -B hw-4/build && cmake --build hw-4/build
./hw-4/build/mutex_test
./hw-4/build/mutex_benchmark

./hw-4/build/mutex_benchmark --benchmark_format=json --benchmark_out=./hw-4/benchmark.json
python3 ./hw-4/check_benchmark.py ./hw-4/benchmark.json
```

## HW-5

```bash
```

## HW-6

```bash
```