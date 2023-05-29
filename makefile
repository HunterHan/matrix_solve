CC=mpicc
WRAPFUNC=-Wl,--wrap=__real_athread_spawn,--wrap=athread_join
CFLAGS=-g -O0 -fPIC -mieee -mftz
INCLUDE=-I.

LIBS=-lswperf
LIB=libmatrix_solve.a
EXE=matrix_solve

all: $(LIB) $(EXE)

$(EXE): main.o host.o slave.o
	mpicxx -g -mhybrid -o $(EXE) $^ -L. -lmatrix_solve -lmpiP -lbfd -liberty -lz

main.o:	main.cpp
	$(CC) -mhost $(CFLAGS) -fpermissive $(INCLUDE) -c $< -o $@ 

host.o: host.cpp
	$(CC) -mhost $(CFLAGS) -fpermissive $(INCLUDE) -c $< -o $@	

slave.o: slave.c
	swgcc -mslave -msimd -Wl,--stack-analysis -Wl,-Map -Wl,mymapfile $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(EXE) *.o
