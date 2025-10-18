#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    printf("[proc_emisor] started with args:\n");
    for (int i = 0; i < argc; ++i)
        printf("  %d: %s\n", i, argv[i]);
    // simulate writing some chars to shared memory (omitted)
    sleep(1);
    printf("[proc_emisor] finished\n");
    return 0;
}