/*
    shell.c
    created by Christian Quintero
    created on 03/22/2026

    This file contains the implementation for shell.h
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define SIZE 1024
#define MAX_TOKENS 32

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
    char cwd[SIZE] = {0};

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

char* get_input(char* input, size_t size)
{
    /*
        this function gets the input line

        :params:
        input: char* - where to store the input into

        :returns:
        input: char* - the input string
    */

    if (fgets(input, size, stdin) == NULL)
    {
        // check if the null is from end of file (Ctrl+D)
        if (feof(stdin))
        {
            printf("Thank you for using the shell!\n");
            exit(0);
        }
        // catch read errors that aren't from EOF
        else
        {
            perror("Error reading input line\n");
            exit(1);
        }
    }

    return input;
}

void tokenize_input(char input[], char* tokens[])
{
    /*
        this function tokenizes the input string into the tokens array
        to separate arguments

        ex) "ls -lah" will be tokenized into
            [0] - "ls"
            [1] - "-lah"

        :params:
        input: char[] - the input string
        tokens: char** - an array of strings representing each token
    */
    int idx = 0;
    char *token = strtok(input, " \t\n");       // tokenize the input by spaces, tabs, and new lines

    // continue to process tokens
    while(token != NULL && idx < MAX_TOKENS - 1)
    {
        tokens[idx++] = token;
        token = strtok(NULL, " \t\n");
    }

    // end the tokens array with a null
    tokens[idx] = NULL;
}

void print_tokens(char* tokens[])
{
    /*
        A debug function to test the tokenizer.

        :params:
        tokens: char* [] - an array of character pointers, aka an array of strings, holding each input token
    */
    int idx = 0;

    printf("Tokens entered:\n");

    while (idx < MAX_TOKENS && tokens[idx] != NULL)
    {
        printf("[%d]: %s\n", idx, tokens[idx]);
        idx++;
    }
}

void run_shell()
{
    /*
        This function contins the main shell loop.
    */
    char input[SIZE] = {0};
    char *tokens[MAX_TOKENS] = {0};

    print_prompt();
    
    while (1)
    {
        print_cwd();
        get_input(input, sizeof(input));
        tokenize_input(input, tokens);
        
        print_tokens(tokens);
    }
}
