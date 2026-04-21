### My Project - Megh Bhavesh Jesalpura 2024CS10844

### COP290 A4 - DataFrameLib

**Recommended build command for graders:**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Certain things that were ensured:

- No memory leaks - ensured using valgrind. Can be checked as:
  `valgrind --leak-check=full --track-origins=yes ./build/your_binary`

- Assuming expressions of the form `col("age") > 30` and fails in expressions `col("age") > lit(30)`
