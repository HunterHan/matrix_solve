# 2023-05-
rows_t可能会爆炸，注意dma分批读取。

这个data的大小，跟batch_size有关系。

dma大小是0时候也会标不对界错误

num的大小可能超标，所以还得再分批

# 2023-05-10
UNIT_SARRAY exception: slave core array, array=0, on cpu vn021582.可能是dma的时候主存地址越界

前5个都能过,6过不了,报`Segmentation fault`

当出现1b/2b时,可能dma一次太大,导致ldm空间不够.

# 2023-05-22
slave core 38: DMA/RMA, vector=0x100, PC=0x4ffff0410800: accessed local LDM with 1B/2B alignment.
Exception Vector=0x20000000000.
可能说明ldm分配空间溢出
注意ldm_malloc不要携程malloc
UNIT_SCORE exception: slave core(s), array=0, core_map=0x8000000000, on cpu vn021545.
Exception Vector=0x20000000000.

ldm_malloc的地址一定记得free

dma，单独0，1可以。但是两次连续等2则不行。

迭代过程中要盯着end_t，小心i月结。


#include<slave.h>的报错
[swliuyao@sw_hpc_103 matrix_solve]$ make
mpicc -mhost -O0 -fPIC -mieee -mftz -fpermissive -I. -c main.cpp -o main.o 
mpicc -mhost -O0 -fPIC -mieee -mftz -fpermissive -I. -c host.cpp -o host.o
mpicc -mslave -msimd -O0 -fPIC -mieee -mftz -c slave.c -o slave.o
In file included from /usr/sw/swgcc/swgcc710-tools-SEA-1307/shared_include/crts2ath.h:2:0,
                 from /usr/sw/swgcc/swgcc710-tools-SEA-1307/shared_include/slave.h:137,
                 from slave.c:1:
/usr/sw/swgcc/swgcc710-tools-SEA-1307/shared_include/crts.h:1468:8: error: expected identifier or '(' before string constant
 extern "C"{
        ^~~
make: *** [slave.o] 错误 1

# 2023-05-24
warning: cpu [21546]: user's mpe task: tid= 0, pid= 26289, exit_code= 1

有些打印消息可能滞后，就异步

make前一定记得保存。实在不行两遍make