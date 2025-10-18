#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    printf("[proc_inicializador] started with args:\n");
    for (int i = 0; i < argc; ++i)
        printf("  %d: %s\n", i, argv[i]);
    sleep(1);
    printf("[proc_inicializador] finished\n");
    return 0;
}