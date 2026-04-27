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
#include <errno.h>
#include <signal.h>

/*
    global constants

    SIZE - the max length of the user input string

    MAX_TOKENS - the max tokens that can be stored. In practice, a NULL is placed after the final valid token.
        If all tokens are filled, then only [0, 30] are stored and [31] is the NULL token
        while if something like "ls -lah" is used, [0] is "ls", [1] is "-lah", and [2] is NULL.
*/
#define SIZE 1024
#define MAX_TOKENS 32

pid_t current_child_pid = -1;

void handle_status(int status);

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

char *get_input(char *input, size_t size)
{
    /*
        this function gets the input line

        :params:
        input: char* - where to store the input into

        :returns:
        input: char* - the input string
    */

    while (1)
    {
        if (fgets(input, size, stdin) != NULL)
        {
            return input;
        }

        // check if the null is from end of file (Ctrl+D)
        if (feof(stdin))
        {
            printf("\nThank you for using the shell!\n");
            exit(0);
        }
        // signal interrupted fgets, just retry the read - Boris Hernandez
        else if (errno == EINTR)
        {
            clearerr(stdin);
            continue;
        }
        // catch read errors that aren't from EOF
        else
        {
            // if reading input fails, then stop the shell
            perror("Error reading input line\n");
            exit(1);
        }
    }
}

void tokenize_input(char input[], char *tokens[])
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
    char *token = strtok(input, " \t\n"); // tokenize the input by spaces, tabs, and new lines

    // continue to process tokens
    while (token != NULL && idx < MAX_TOKENS - 1)
    {
        tokens[idx++] = token;
        token = strtok(NULL, " \t\n");
    }

    // end the tokens array with a null
    tokens[idx] = NULL;
}

void print_tokens(char *tokens[])
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

void sigint_handler(int sig)
{
    /*
        Handles SIGINT (Ctrl+C) by forwarding it to the child process
        instead of terminating the shell.

        :params:
        sig: int - the signal number (SIGINT)
    */
    if (current_child_pid > 0)
    {
        if (kill(current_child_pid, SIGINT) == -1)
        {
            perror("Error sending SIGINT to child");
        }
    }
    else
    {
        // no child running, print newline
        write(STDOUT_FILENO, "\n", 1);
        print_cwd();
        fflush(stdout);
    }
}

void sigtstp_handler(int sig)
{
    /*
        Handles SIGTSTP (Ctrl+Z) by forwarding it to the child process
        instead of stopping the shell.

        :params:
        sig: int - the signal number (SIGTSTP)
    */
    if (current_child_pid > 0)
    {
        if (kill(current_child_pid, SIGTSTP) == -1)
        {
            perror("Error sending SIGTSTP to child");
        }
    }
}

void sigchld_handler(int sig)
{
    /*
        Handles SIGCHLD to clean up zombie processes when a child terminates.
        Uses waitpid with WNOHANG to reap any available child processes.

        Only resets current_child_pid — does NOT call handle_status() because
        run_command's waitpid() already handles that, so we avoid calling it twice.

        :params:
        sig: int - the signal number (SIGCHLD)
    */

    if (current_child_pid == -1)
    {
        return;
    }

    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (pid == current_child_pid)
        {
            current_child_pid = -1;
        }
    }
}

void setup_signal_handlers()
{
    /*
        Sets up signal handlers for SIGINT, SIGTSTP, and SIGCHLD
        using sigaction() with proper error checking.
    */
    struct sigaction sa_int, sa_tstp, sa_chld;

    // Setup SIGINT handler
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sa_int.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("Error setting up SIGINT handler");
        exit(1);
    }

    // Setup SIGTSTP handler
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sa_tstp.sa_handler = sigtstp_handler;

    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1)
    {
        perror("Error setting up SIGTSTP handler");
        exit(1);
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
        fprintf(stderr, "\nChild terminated by signal: %d\n", signal);
    }
    else
    {
        printf("Child terminated abnormally\n");
    }
}

void run_command(char *tokens[])
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

        // changing directories was successful
        // so return to prevent fork calls
        return;
    }
    else if (strcmp(tokens[0], "exit") == 0)
    {
        // exit is a built in that doesn't require fork
        printf("Thank you for using the shell!\n");
        exit(0);
    }

    if ((child_pid = fork()) == -1)
    {
        // instead of ending the app, just return to the shell loop
        perror("Error calling fork: command could not be ran\n");
        return;
    }

    if (child_pid == 0)
    {
        // child block

        // reset signal handlers to default so child behaves normally - Boris Hernandez
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // the child process executes the command
        // code after execvp is only ran if execvp fails
        execvp(tokens[0], tokens);

        // check why execvp failed
        switch (errno)
        {
        case EACCES:
            // 126 is the common exit code for permission denied
            fprintf(stderr, "%s: permission denied\n", tokens[0]);
            exit(126);
            break;
        case ENOENT:
            // 127 is the common exit code for command not found
            fprintf(stderr, "%s: command not found\n", tokens[0]);
            exit(127);
            break;
        default:
            // execvp failed due to an unknown reason
            perror("call to execvp failed");
            exit(1);
            break;
        }
    }
    else
    {
        // parent block
        current_child_pid = child_pid;
        waitpid(child_pid, &status, 0);
        current_child_pid = -1;
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

    setup_signal_handlers();
    print_prompt();

    while (1)
    {
        print_cwd();
        get_input(input, sizeof(input));
        tokenize_input(input, tokens);
        run_command(tokens);
    }
}

void run_shell_from_file(const char *filename)
{
    /*
        This function reads commands from a file and executes them
        one line at a time, instead of reading from user input.

        :params:
        filename: const char* - the path to the script file
    */
    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    char input[SIZE] = {0};
    char *tokens[MAX_TOKENS] = {0};

    while (fgets(input, sizeof(input), fp) != NULL)
    {
        // strip both \n and \r (handles Windows CRLF line endings)
        input[strcspn(input, "\r\n")] = '\0';

        // skip empty lines and comment lines starting with #
        if (input[0] == '\0' || input[0] == '#')
            continue;

        // echo the command so you can see what's being run
        printf("%s$ %s\n", filename, input);

        tokenize_input(input, tokens);
        run_command(tokens);
    }

    fclose(fp);
    printf("\nFile execution complete.\n");
}