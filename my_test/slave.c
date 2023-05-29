#include <slave.h>
#include <athread.h>

__thread_local unsigned int dma_rply = 0;
__thread_local unsigned int D_COUNT = 0;

int solve(int x)
{
    return x / x / x / x / x / x / x / x / x / x / x / x / x / x;
}

void fun(int *a)
{
    //     int a_t[2];
    //     dma_rply = 0;
    //     athread_dma_iget_stride(a_t, a, 2* sizeof(int), sizeof(int), 4*sizeof(int) , &dma_rply);
    //     athread_dma_wait_value(&dma_rply, 1);
    //     for(size_t i = 0; i < 5; i++){
    //         printf("a_t[%d] = %d\n",i,a_t[i]);
    //     }

    int b = 1;
    for (size_t i = 0; i < 1000000; i++)
    {
        b += solve(b);
    }
}