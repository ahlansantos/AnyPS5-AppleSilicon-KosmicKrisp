#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char** argv) {
    void* h = dlopen(argv[1], RTLD_LAZY);
    if (!h) { printf("dlopen failed\n"); return 1; }
    void* p = dlsym(h, "bar");
    if (!p) { printf("dlsym failed: %s\n", dlerror()); return 1; }
    printf("dlsym success!\n");
    return 0;
}
