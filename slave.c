#include <slave.h>
#include "matrix_def.h"
#include <math.h>
#include <simd.h>
#include <athread.h>

#define thread_num 64
#define x_cache_thread_num 50
#define MAX_DMA_NUM 5000
#define MAX_COL_NUM 5000
#define MAX_ROWS 200000

typedef struct Args
{
	int rows;
	int data_size;
	// Csr
	int *row_off;
	int *cols;
	double *data;

	double *x;
	double *b;
} Args;

#if 1

__thread_local_share double x_cache[MAX_ROWS];

// index:7
__thread_local int mesh_size_t;

__thread_local athread_rply_t dma_rply;
__thread_local athread_rply_t rma_rply;
__thread_local unsigned int D_COUNT = 0;
__thread_local unsigned int R_COUNT = 0;

// __thread_local const int mesh_size_t = mesh_size / thread_num;
__thread_local int rows;
__thread_local int *row_off;
__thread_local int *cols;
__thread_local double *data;
__thread_local double *x_;
__thread_local double *b_;

__thread_local int *row_off_s;
__thread_local double *x_t;
__thread_local int task_global[thread_num][2];
__thread_local int row_off_t[MAX_DMA_NUM + 1];
__thread_local int cols_t[MAX_COL_NUM];
__thread_local double data_t[MAX_COL_NUM];
__thread_local double b_t[MAX_DMA_NUM];

__thread_local double x_local = -1;

// 从其他从核缓存局部x
// x_cache_t相对全局x的偏移
__thread_local int x_t_offset = 0;
// 当前缓存数据所在的从核id号
__thread_local int x_t_offset_myid = 0;

double get_x(int col)
{
	/**
	 * @brief 直接根据全局，算出来从哪个核组拿数据.
	 * 这个方法一已经验证过参数col的正确性。如果出问题，应该就是方法二自己本身的问题。
	 */
#if 1
// 方法三：直接从ldm连续共享段获取
	if (col >= rows)
	{
		dma_rply = 0;
		athread_dma_iget(&x_local, &x_[col], sizeof(double), &dma_rply);
		athread_dma_wait_value(&dma_rply, 1);
		return x_local;
	}else{
		return x_cache[col];	
	}
	
#else
// 方法一：dma拿x
	dma_rply = 0;
	athread_dma_iget(&x_local, &x_[col], sizeof(double), &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);

// 方法二：LDM拿x
	// 如果是比较奇葩的x的id（就很大的那种），单独dma
	if (col >= rows)
	{
		dma_rply = 0;
		athread_dma_iget(&x_local, &x_[col], sizeof(double), &dma_rply);
		athread_dma_wait_value(&dma_rply, 1);
	}

	int rid = col / mesh_size_t;
	int offset = col % mesh_size_t;

	if (rid != _MYID)
	{
		// 如果x不在本CPE上，从其他CPE拿
		rma_rply = 0;
		// printf("col:%d,rid:%d,offset:%d\n",col,rid,offset);
		athread_rma_iget(&x_local, &rma_rply, sizeof(double), rid, &x_t[offset], &rma_rply);
		athread_rma_wait_value(&rma_rply, 1);
	}
	else
	{
		// 如果x在本CPE上，直接拿
		x_local = x_t[offset];
	}
#endif

	return x_local;
}

void intra_row_aligned_solver(int *start_t_ptr,int batch_num,double *result_ptr){
// printf("batch_begin_t:%d,i:%d,start:%d,num:%d,batch_end_t:%d\n", batch_begin_t, i, start, num, batch_end_t);
	dma_rply = 0;
	// athread_dma_iget(cols_t, &cols[start_t], batch_num * sizeof(int), &dma_rply);
	athread_dma_iget(cols_t, &cols[*start_t_ptr], batch_num * sizeof(int), &dma_rply);

	athread_dma_wait_value(&dma_rply, 1);

	dma_rply = 0;
	// athread_dma_iget(data_t, &data[start_t], batch_num * sizeof(double), &dma_rply);
	athread_dma_iget(data_t, &data[*start_t_ptr], batch_num * sizeof(double), &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);

	for (int j = 0; j < batch_num; j++)
	{
		// result += get_x(cols_t[j]) * data_t[j];
		*result_ptr += get_x(cols_t[j]) * data_t[j];
	}

	// start_t += batch_num;
	*start_t_ptr += batch_num;
}

void intra_row_tail_solver(int *start_t_ptr,int batch_num,double *result_ptr){
	
// printf("batch_begin_t:%d,i:%d,start:%d,num:%d,batch_end_t:%d\n", batch_begin_t, i, start, num, batch_end_t);
	dma_rply = 0;
	// athread_dma_iget(cols_t, &cols[start_t], batch_num * sizeof(int), &dma_rply);
	athread_dma_iget(cols_t, &cols[*start_t_ptr], batch_num * sizeof(int), &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);

	dma_rply = 0;
	// athread_dma_iget(data_t, &data[start_t], batch_num * sizeof(double), &dma_rply);
	athread_dma_iget(data_t, &data[*start_t_ptr], batch_num * sizeof(double), &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);

	for (int j = 0; j < batch_num; j++)
	{
		// result += get_x(cols_t[j]) * data_t[j];
		*result_ptr += get_x(cols_t[j]) * data_t[j];
		// if(data_t[j] != data_t[j]){
		// 	printf("data_t nan\n");
		// }
		// if(get_x(cols_t[j]) != get_x(cols_t[j])){
		// 	printf("x nan\n");
		// }
		// if(*result_ptr != *result_ptr){
		// 	printf("result nan\n");
		// }
	}

	// start_t += batch_num;
	*start_t_ptr += batch_num;
}

void solver(int batch_begin_t, int batch_end_t, int batch_dma_num)
{
	// begin 9844,会出错
	// printf("batch_begin_t:%d,batch_end_t:%d\n",batch_begin_t,batch_end_t);
	// dma访存拿数据
	
	dma_rply = 0;
	athread_dma_iget(row_off_t, &row_off[batch_begin_t], sizeof(int) * (batch_dma_num + 1), &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);


	// 计算。遍历每行
	for (size_t i = 0; i < batch_dma_num; i++)
	{
		
		int intra_row_start = row_off_t[i];
		int intra_row_num = row_off_t[i + 1] - row_off_t[i];
		int intra_row_end = intra_row_start + intra_row_num;

		// if(intra_row_num > MAX_COL_NUM){
		// 	printf("batch_begin_t:%d,batch_dma_num_i:%d,intra_row_start:%d,intra_row_end:%d\n",batch_begin_t,i,intra_row_start,intra_row_end);
		// }
		int intra_row_start_t = intra_row_start;
		int intra_row_batch_num = MAX_COL_NUM;
		
		// printf("batch_begin_t:%d,i:%d,start:%d,num:%d\n",batch_begin_t,i,start,num);

		double result = .0;
		// MY_BUG:这里又是，+写成了-
		while (intra_row_start_t + intra_row_batch_num < intra_row_end)
		{
			intra_row_aligned_solver(&intra_row_start_t, intra_row_batch_num, &result);
		}
		intra_row_batch_num = intra_row_end - intra_row_start_t;
		if (intra_row_batch_num > 0)
		{
			intra_row_tail_solver(&intra_row_start_t, intra_row_batch_num, &result);
		}
		b_t[i] = result;
	}
#if 1
	// b写回
	dma_rply = 0;
	athread_dma_iput(&b_[batch_begin_t], b_t, sizeof(double) * batch_dma_num, &dma_rply);
	athread_dma_wait_value(&dma_rply, 1);
#endif

}

#endif

void task_divide()
{
#if 0
		// 从主存跨步读，尽量保证负载均衡即可。
		int stride = rows / thread_num;
		int stride_num = 1 + rows / stride;

		dma_rply = 0;
		row_off_s = (int *)ldm_malloc(sizeof(int) * stride_num);
		athread_dma_iget_stride(row_off_s, row_off, stride_num * sizeof(int), sizeof(int), sizeof(int) * (stride - 1), &dma_rply);
		athread_dma_wait_value(&dma_rply, 1);
		// 任务负载划分
		int idx = 1;
		task_global[0][0] = 0;
		task_global[thread_num - 1][1] = row_off_s[stride_num - 1];

		for (size_t i = 0; i < stride_num && idx < thread_num; i++)
		{
			if (row_off_s[i] >= data_size_t * idx)
			{
				// printf("row off :%d! when idx = %d!\n",row_off_s[i],idx);
				task_global[idx - 1][1] = i * stride + 1;
				if (idx < thread_num)
				{
					task_global[idx][0] = i * stride;
				}
				idx++;
			}
		}
		ldm_free(row_off_s, sizeof(int) * stride_num);
#else
	int task_assign = rows / thread_num;
	int task_tail = rows % thread_num;
	for (size_t i = 0; i < task_tail; i++)
	{
		task_global[i][0] = i * (task_assign + 1);
		task_global[i][1] = task_global[i][0] + (task_assign + 1);
	}
	for (size_t i = task_tail; i < thread_num; i++)
	{
		task_global[i][0] = task_tail * (task_assign + 1) + (i - task_tail) * task_assign;
		task_global[i][1] = task_global[i][0] + task_assign;
	}
	// for(size_t i = 0 ; i < thread_num;i++){
	// 	printf("task_global[%d]:%d-%d\n",i,task_global[i][0],task_global[i][1]);
	// }
#endif
}

void spmv_solver(Args *args_t)
{

	rows = args_t->rows;
	int data_size_t = args_t->data_size / thread_num;
	row_off = args_t->row_off;
	data = args_t->data;
	cols = args_t->cols;
	x_ = args_t->x;
	b_ = args_t->b;

	// 初始化
	if (_MYID == 0)
	{
		// 任务划分
		task_divide();
		// 任务划分广播
		rma_rply = 0;
		athread_rma_ibcast(&(task_global[0][0]), &(task_global[0][0]), &rma_rply, thread_num * 2 * sizeof(int), &rma_rply);
		athread_rma_wait_value(&rma_rply, 1);
		// 连续共享空间
		// 似乎连续共享段无法用ldm_malloc再分配，必须一开始就给定。
		// x_cache = (double*)ldm_malloc(sizeof(double)*rows);
		// x_cache用dma能拿，但是只能拿一个？
#if 0
		//从主存将x拿到ldm连续共享段
		dma_rply = 0;
		// 最多12个可以.16个就不行
		// 实在不行先拿到局存，再去搞别的吧
		dma:sizeof(double)*16(X)->__thread_local_share x
		->__thread_local OK
		DMA->__thread_local
		thread_local -> ..share
		int x_cache_batch_num = 1;
		int x_cache_batch_begin = 0;
		while(x_cache_batch_begin < MAX_ROWS){
			printf("x_cache_batch_begin:%d\n",x_cache_batch_begin);
			athread_dma_iget(&data_t[x_cache_batch_begin],&x_[x_cache_batch_begin],sizeof(double)*x_cache_batch_num,&dma_rply);
			athread_dma_wait_value(&dma_rply,1);
			x_cache_batch_begin += x_cache_batch_num;
		}
#endif
		
	}
	
#if 1
	// 花8个cpe单独缓存x
	if (_MYID < x_cache_thread_num)
	{
		// load_x
		mesh_size_t = rows / x_cache_thread_num;
		x_t = (double *)ldm_malloc(sizeof(double) * mesh_size_t);
		dma_rply = 0;
		athread_dma_iget(x_t, &x_[_MYID * mesh_size_t], mesh_size_t * sizeof(double), &dma_rply);
		athread_dma_wait_value(&dma_rply, 1);
		
		for(size_t i = 0; i < mesh_size_t; i++){
			x_cache[i + _MYID * mesh_size_t] = x_t[i];
		}
		
	}
	athread_ssync_array();
#endif
	
	// athread_ssync_array();

	// 获取myid对应任务 thread_global[_MYID][0] ---> thread_global[_MYID][1]
	int begin_t = task_global[_MYID][0];
	int end_t = task_global[_MYID][1]; // 不包括本行
	int batch_dma_num = MAX_DMA_NUM;

	// MY_BUG：这里+写成-
	while (begin_t + MAX_DMA_NUM < end_t)
	{
		solver(begin_t, end_t, batch_dma_num);
		begin_t += MAX_DMA_NUM;
	}

	batch_dma_num = end_t - begin_t;
	if (batch_dma_num > 0)
	{
		solver(begin_t, end_t, batch_dma_num);
	}

	if (_MYID < 50)
	{
		ldm_free(x_t, sizeof(double) * mesh_size_t);
	}
	return;
}