#include <athread.h>
#include "matrix_def.h"
#include "host.h"

#include "mpi.h"

typedef struct Args{
    int rows;
    int data_size;
    // Csr
    int* row_off;
    int* cols;
    double* data;

    double* x;
    double* b;
}Args;

extern "C" void SLAVE_FUN(spmv_solver)(Args *);

void spmv(const CsrMatrix &csr_matrix, double *x, double *b) {
    // 实现样例
#if 0
    int max_col = 0;
    for(int i = 0; i < csr_matrix.rows; i++) {
        int start = csr_matrix.row_off[i];
        int num = csr_matrix.row_off[i+1] - csr_matrix.row_off[i];
        double result = .0;
        for(int j = 0; j < num; j++) {
            if(csr_matrix.cols[start+j] > max_col){
                max_col = csr_matrix.cols[start+j];
            }
            result += x[csr_matrix.cols[start+j]] * csr_matrix.data[start+j];
        }
        b[i]=result;
    }
    // printf("%d\n",max_col);
#else
    // printf("csr_matrix.data_size:%d\n",csr_matrix.data_size);
    // printf("csr_matrix.row_off[-1]:%d\n",csr_matrix.row_off[csr_matrix.rows]);
    // return ;
    //众核版本
    Args args;
    args.rows = csr_matrix.rows;
    args.data_size = csr_matrix.data_size;

    args.row_off = csr_matrix.row_off;
    args.cols = csr_matrix.cols;
    args.data = csr_matrix.data;
    args.x = x;
    args.b = b;
    auto array_id = current_array_id();
    // printf("array_id:%d,rows:%d\n",array_id,args.rows);
    // printf("array_id:%d,ror_off:%p\n",array_id,args.row_off);
    // printf("array_id:%d,cols:%p\n",array_id,args.cols);
    // printf("array_id:%d,data:%p\n",array_id,args.data);
    
    athread_init();
    athread_spawn(slave_spmv_solver,&args);
    athread_join();
    athread_halt();
#endif

#if 0
    int my_rank{0};
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    for(size_t i = 0; i < 10; i++){
        printf("rank:%d, b[%d]:%lf\n",my_rank, i,b[i]);
    }
#endif

}
