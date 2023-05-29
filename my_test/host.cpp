#include "athread.h"

extern "C" void SLAVE_FUN(fun)(int *);

int main(int atgc, char** argv){

    int a[10] = {0,1,2,3,4,5,6,7,8,9};

    athread_init();
    athread_spawn(slave_fun, a);

    athread_join();
    athread_halt();

    return 0;
}