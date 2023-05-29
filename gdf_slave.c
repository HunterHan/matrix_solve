#include <slave.h>
#include "matrix_def.h"
#include <math.h>
#include <simd.h>
#include<athread.h>

#define thread_num 64
__thread_local athread_rply_t dma_rply;
__thread_local unsigned int DMA_COUNT = 0;
extern double *x;
extern double *b;

void sw_slave_func(struct CsrMatrix *csr_matrix) {
	int all_row=csr_matrix->rows;
	int convey_row=all_row/thread_num;
	int remainder=all_row%thread_num;
	int add_row=0;
	int begin=0;
	int x_size=all_row*6/thread_num;
	int remainder_xsize=(all_row*6)%thread_num;
	double local_x[x_size+remainder_xsize];
	if(_MYID!=63){
		athread_dma_iget(local_x,&(x[_MYID*x_size]),x_size*sizeof(double),&dma_rply);
		DMA_COUNT++;
	}else{
		//printf("all_row=%d, get_allocatable_size=%d,x_zie=%d,myid=%d,all_row*6-63*x_size=%d \n",all_row,get_allocatable_size(),x_size,_MYID,all_row*6-63*x_size);
		athread_dma_iget(local_x,&(x[_MYID*x_size]),(x_size+remainder_xsize)*sizeof(double),&dma_rply);
		DMA_COUNT++;
		//printf("all_row=%d, get_allocatable_size=%d,x_zie=%d,myid=%d,all_row*6-63*x_size=%d \n",all_row,get_allocatable_size(),x_size,_MYID,all_row*6-63*x_size);
	}

	if(_MYID<remainder){
		convey_row+=1;
		add_row=_MYID;
		begin=convey_row*_MYID;
	}else{
		add_row=remainder;
		begin=convey_row*_MYID+remainder;
	}

	int row_off[convey_row+1];
	
	//x不能用LDM存，因为太大，对称矩阵行列相同，如果存入LDM 需要 120w*8字节的空间。实际可用只有223744。
	//共享LDM为0，因此不能使用这种方式
	begin= add_row+all_row/thread_num;
	double local_b[convey_row];
	athread_dma_iget(row_off,&(csr_matrix->row_off[begin]),(convey_row+1)*sizeof(int),&dma_rply);
	DMA_COUNT++;
    athread_dma_wait_value(&dma_rply,DMA_COUNT);

  for (int i =0; i <convey_row; i++)
    {
		int total=0;
		int head=i;
		int allow_size_number=(get_allocatable_size()-256)/12;
		while (i<convey_row && allow_size_number<allow_size_number){
			i++;
			total=total+row_off[i]-row_off[i-1];	
		}
		if(total>allow_size_number){
			total=total-(row_off[i]-row_off[i-1]);
			i--;
		}
		
        //int num = row_off[i + 1] - row_off[i];//当前计算行计算的元素数
        double result = 0.0;
        int cols[total];//用于存储当前计算行索引
        double datas[total]; //用于存储当前计算行数组
        athread_dma_iget(cols,&(csr_matrix->cols[row_off[head]]), total * sizeof(int), &dma_rply);
        athread_dma_iget(datas,&(csr_matrix->data[row_off[head]]), total * sizeof(double), &dma_rply);
        DMA_COUNT+=2;
        athread_dma_wait_value(&dma_rply,DMA_COUNT);

        for (int j = 0; j < i-head; j++)
        {    
			int start = row_off[head+j]-row_off[head];
        	int num = row_off[head+j+1] - row_off[head+j];
        	double result = .0;
			for(int k = 0; k < num; k++) {
				double temp;
				int l=0;
				for(l=0;l<64;l++){
					 if( row_off[head+j]+k < l* x_size){
						break;
					 }
					 l--;
					 int position=row_off[head+j]+k - l*_MYID;
					athread_rma_iget(&temp,&dma_rply,sizeof(double),k,&(local_x[position]),&dma_rply);
					DMA_COUNT++;
					athread_rma_wait_value(&dma_rply,DMA_COUNT);
				}
				// athread_dma_iget(&temp,&(x[cols[start+k]]),sizeof(double),&dma_rply);
				// ++DMA_COUNT;
				// athread_dma_wait_value(&dma_rply,DMA_COUNT);
				result += temp * datas[start+k];
				//printf("temp=%f,cols[start+k]=%d, k=%d, datas[start+k]=%d  start = %d \n",temp,cols[start+k],k, datas[start+k],start);
			}   
          
            local_b[i] = result;
            
    }
	}
  

    // 将结果写回b向量
      athread_dma_iput(&(b[convey_row]),local_b, convey_row * sizeof(double), &dma_rply);
      DMA_COUNT++;
      athread_dma_wait_value(&dma_rply, DMA_COUNT);
}