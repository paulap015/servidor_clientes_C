
all: servidor.o cliente.o netutil.o split.o
	gcc -o servidor servidor.o netutil.o split.o -pthread
	gcc -o cliente cliente.o netutil.o split.o -pthread	
	

servidor.o: servidor.c
	gcc -c -o servidor.o servidor.c

cliente.o: cliente.c
	gcc -c -o cliente.o cliente.c

netutil.o: netutil.c
	gcc -c -o netutil.o netutil.c

split.o: split.c
	gcc -c -o split.o split.c
clean:
	rm -f servidor *.o
	rm -f cliente *.o

