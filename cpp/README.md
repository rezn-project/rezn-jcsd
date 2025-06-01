## Building the CLI tool

Builds a simple canonicalization CLI:

```bash
g++ -std=c++20 -O2 -Iexternal/json/single_include main.cpp -o jcs
```

Example usage:

```bash
cat input.json | ./jcs
```

## Building the Shared Library (.so)

Compiles the reusable shared object (`.so`) for dynamic linking (e.g. for `rezn-dsl` FFI):

```bash
g++ -std=c++20 -O2 -fPIC -shared -Iexternal/json/single_include reznjcs.cpp -o libreznjcs.so
```

This exports a single C-style function:

```cpp
extern "C" const char* rezn_canonicalize(const char* input_json);
```

## Running the Tests

Build and run the standalone test runner:

```bash
g++ -std=c++20 -O2 -Iexternal/json/single_include test.cpp -o jcs-test
./jcs-test
```

---

Optional section you might want to add:

## Linking the `.so` from another language

Example for linking from C:

```c
extern const char* rezn_canonicalize(const char* input_json);

// Usage example:
const char* result = rezn_canonicalize(input_json);
if (result != NULL) {
    // Use the result...
    printf("%s\n", result);
    free((void*)result);  // Important: free the allocated memory
}
```

Compile with:

```bash
gcc myapp.c -L. -lreznjcs -o myapp
```

Run with:

```bash
LD_LIBRARY_PATH=. ./myapp
```

