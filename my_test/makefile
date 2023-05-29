CC=swgcc

CFLAGS=-O0 -fPIC -mieee -mftz
INCLUDE=-I.

EXE=test
INCLUDE=-I.

all: $(EXE)

$(EXE): host.o slave.o
	$(CC) -mhybrid -o $(EXE) $^

host.o: host.cpp
	$(CC) -mhost $(CFLAGS) -fpermissive $(INCLUDE) -c $< -o $@	

slave.o: slave.c
	$(CC) -mslave -msimd $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	$(RM) $(EXE) *.o
