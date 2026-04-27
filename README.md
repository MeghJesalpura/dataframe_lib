# My Project - Megh Bhavesh Jesalpura - 2024CS10844

### COP290 A4 - DataFrameLib

**Recommended build command:**

```bash
cmake -S . -B build
cmake --build build
```

This command as given in the autograder will work

```bash
python autograder.py --student-dir /path/to/student/project
```

## Things required

### Core build requirements

```bash
CMake >= 3.25
C++ compiler with C++17 support (g++/clang++)
Arrow
ArrowDataset
Parquet
```

### Python requirements (for tester/autograder)

```bash
Python 3.9+
pandas>=2.0
pyarrow>=12.0
numpy>=1.24
```

### Runtime/tool requirement

```bash
Graphviz (dot)  # needed for LazyDataFrame::explain()
```

### Checker-facing project requirements

```bash
CMake target name: dataframelib
Namespace: dataframelib
```

## Certain things that were ensured:

- No memory leaks - ensured using valgrind. Can be checked as:
  `valgrind --leak-check=full --track-origins=yes ./build/your_binary`

- RAII, smart pointers, move semantics used

- Also note that public APIs were documented as per requirement.
