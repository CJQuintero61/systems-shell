/*
    shell.c
    created by Christian Quintero
    created on 03/22/2026

    This file contains the implementation for shell.h
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIZE 1024

void print_prompt()
{
    /*
        This function prints the welcome message to the shell upon starting the program.
        The prompt will be updated to be better soon.
    */
    char prompt[] = "Welcome to the shell!";
    printf("%s\n\n", prompt);
}

void print_cwd()
{
    /*
        This function prints the current working directory (CWD) to
        provide a shell experience
    */
    char cwd[SIZE];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s$ ", cwd);
    }
    else
    {
        perror("Failed to print working directory\n");
        exit(1);
    }
}


void run_shell()
{
    /*
        This function contins the main shell loop.
    */
    char input[SIZE];

    print_prompt();
    
    while (1)
    {
        print_cwd();
        fgets(input, sizeof(input), stdin);     // read input line

        printf("You entered %s\n", input);
    }
}
