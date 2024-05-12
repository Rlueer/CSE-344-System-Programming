all: clean compile run
compile: main.c
	@gcc -o a.out main.c -lrt
clean:
	@rm -f *.out
	@rm -f fifo1
	@rm -f fifo2
run: 
	@./a.out
