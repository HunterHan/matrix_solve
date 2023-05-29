set -x

bsub -b -I -q q_sw_expr -n 6 -np 1 -cgsp 64 -host_stack 1024 -share_size 15000 -ldm_share_mode 4 -ldm_share_size 64 -swrunarg "-P slave" -J hzy_matrix_solve -perf ./matrix_solve  2>&1 | tee ./my_logs/2023-05-22-test.log
