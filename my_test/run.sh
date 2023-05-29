set -x

bsub -b -I -q q_sw_expr -n 1 -np 1 -cgsp 64 -host_stack 1024 -share_size 15000 -swrunarg -p -J hzy_matrix_solve ./test