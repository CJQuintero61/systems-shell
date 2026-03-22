.PHONY: all
all: main

# make the target main by using all dependencies
main: main.o shell/shell.o
	gcc -o $@ $^

# make the main.o object file
main.o: main.c shell/shell.h
	gcc -c $< -o $@

# make the shell.o object file
shell/shell.o: shell/shell.c shell/shell.h
	gcc -c $< -o $@

# remove the executable and object files
.PHONY: clean
clean:
	rm -f main main.o shell/*.o