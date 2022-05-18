#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int main(int argc, char const *argv[])
{
    
    struct timeval x;
    long aux;

    gettimeofday (&x, NULL);
    aux = x.tv_sec * (10^6) + x.tv_usec;

    printf ("%ld\n", aux);

    return 0;
}
