
#include <stdlib.h>
#include <stdio.h>

#define nullptr ((void *)0)
#define NOT_IMPLEMENTED(...) fprintf(stderr,  "%s:%d: Error: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE)
#define ASSERT(b, ...) if (!(b)) { fprintf(stderr,  "%s:%d: Assertion failed: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); }
