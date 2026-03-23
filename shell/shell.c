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
#include <sys/wait.h>

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
        // if this fails, then stop the shell
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
            printf("\nThank you for using the shell!\n");
            exit(0);
        }
        // catch read errors that aren't from EOF
        else
        {
            // if reading input fails, then stop the shell
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

void handle_status(int status)
{
    /*
        checks the status code of a child process

        :params:
        status: int - the status code of the child
    */
    int signal;

    if (WIFEXITED(status))
    {
        // we don't need to handle proper termination
    }
    else if (WIFSIGNALED(status))
    {
        signal = WTERMSIG(status);
        fprintf(stderr, "Child termianted by signal: %d\n", signal);
    }
    else
    {
        printf("Child terminated abnormally\n");
    }
}

void run_command(char* tokens[])
{
    /*
        this function runs a command by calling fork
        and having the child process run the command
        while the parent waits

        :params:
        tokens: char** - the tokens array
    */
    pid_t child_pid = 0;
    int status;

    // special cases
    if (tokens[0] == NULL)
    {
        // if the user just hit space, do nothing
        return;
    }
    else if (strcmp(tokens[0], "cd") == 0)
    {
        // changing directories affects the main shell process
        // so do not call fork for this

        if (tokens[1] == NULL)
        {
            fprintf(stderr, "missing directory path argument\n");
            return;
        }

        if ((chdir(tokens[1])) == -1)
        {
            // will be printed if the specified directory doesn't exist
            // or if chdir() fails
            perror("failed to change directories");
            return;
        }

        // prevent fork calls
        return;
    }

    if((child_pid = fork()) == -1)
    {
        // instead of ending the app, just return to the shell loop
        perror("Error calling fork: command could not be ran\n");
        return;
    }
    
    if (child_pid == 0)
    {
        return;
    }
    else
    {
        // parent block
        waitpid(child_pid, &status, 0);
        handle_status(status);
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
        run_command(tokens);
    }
}
