FLAGS=-m32 -O2

all: interpreter dump

interpreter: interpreter.o bytefile.o ../runtime/runtime.a
	$(CC) $(FLAGS) -o interpreter interpreter.o bytefile.o ../runtime/runtime.a

dump: dump.o bytefile.o ../runtime/runtime.a
	$(CC) $(FLAGS) -o dump dump.o bytefile.o ../runtime/runtime.a

%.o: %.c
	$(CC) $(FLAGS) -c $<

clean:
	$(RM) *.a *.o *~ interpreter dump
