/*
Christian Quintero, Anointiyae Beasley, Venus Ubani, Boris Hernandez
Systems Programming Project - Spring 2026
03/22/2026

to run (interactive mode):
1. make all
2. ./main
3. make clean

to run (file mode):
1. make all
2. ./main yourfile.txt
3. make clean

This program implements a shell in C
*/
#include <stdio.h>
#include "shell/shell.h"

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        // a filename was passed as an argument — run in file mode
        run_shell_from_file(argv[1]);
    }
    else
    {
        // no argument — run in normal interactive mode
        run_shell();
    }

    return 0;
}