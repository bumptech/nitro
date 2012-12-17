redo-ifchange basic.o ../libnitro.a
gcc -L.. basic.o -lnitro -luv -lpthread -o $3
