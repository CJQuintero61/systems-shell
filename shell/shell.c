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
#include <fcntl.h>
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

static void print_help(void);
static int is_background_command(char *tokens[]);
static void strip_background_token(char *tokens[]);
static int validate_single_command(char *tokens[]);
static int validate_tokens(char *tokens[]);
static void execute_pipeline(char *tokens[], int background);

int has_pipe(char *tokens[])
{
    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
            return 1;
    }
    return 0;
}

int has_redirection(char *tokens[])
{
    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0)
            return 1;
    }
    return 0;
}

static int is_background_command(char *tokens[])
{
    /*
        checks if the user wants to run the command in the background

        this only counts if & is the last token
    */
    int last = -1;

    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "&") == 0 && tokens[i + 1] != NULL)
        {
            return 0;
        }
        last = i;
    }

    return last >= 0 && strcmp(tokens[last], "&") == 0;
}

static void strip_background_token(char *tokens[])
{
    /*
        removes the ending & so execvp does not see it as a normal argument
    */
    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "&") == 0 && tokens[i + 1] == NULL)
        {
            tokens[i] = NULL;
            return;
        }
    }
}

static int validate_single_command(char *tokens[])
{
    /*
        checks one command for bad syntax like missing redirect files
        or multiple redirects of the same type
    */
    int saw_input = 0;
    int saw_output = 0;

    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            fprintf(stderr, "syntax error: unexpected pipe in command segment\n");
            return 0;
        }

        if (strcmp(tokens[i], "&") == 0)
        {
            if (tokens[i + 1] != NULL)
            {
                fprintf(stderr, "syntax error: '&' must appear only at the end\n");
                return 0;
            }
        }

        if (strcmp(tokens[i], "<") == 0)
        {
            if (saw_input)
            {
                fprintf(stderr, "syntax error: multiple input redirections\n");
                return 0;
            }

            if (tokens[i + 1] == NULL)
            {
                fprintf(stderr, "Missing file for redirection\n");
                return 0;
            }

            if (strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "&") == 0)
            {
                fprintf(stderr, "syntax error: invalid token after '<'\n");
                return 0;
            }

            saw_input = 1;
            i++;
        }
        else if (strcmp(tokens[i], ">") == 0)
        {
            if (saw_output)
            {
                fprintf(stderr, "syntax error: multiple output redirections\n");
                return 0;
            }

            if (tokens[i + 1] == NULL)
            {
                fprintf(stderr, "Missing file for redirection\n");
                return 0;
            }

            if (strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "&") == 0)
            {
                fprintf(stderr, "syntax error: invalid token after '>'\n");
                return 0;
            }

            saw_output = 1;
            i++;
        }
    }

    return 1;
}

static int validate_tokens(char *tokens[])
{
    /*
        checks the full token list before trying to run anything

        this helps catch bad pipe usage and bad background syntax
    */
    if (tokens[0] == NULL)
    {
        return 1;
    }

    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "&") == 0 && tokens[i + 1] != NULL)
        {
            fprintf(stderr, "syntax error: '&' must appear only at the end\n");
            return 0;
        }
    }

    if (!has_pipe(tokens))
    {
        return validate_single_command(tokens);
    }

    if (strcmp(tokens[0], "|") == 0)
    {
        fprintf(stderr, "syntax error: leading pipe\n");
        return 0;
    }

    int start = 0;

    for (int i = 0;; i++)
    {
        if (tokens[i] == NULL || strcmp(tokens[i], "|") == 0)
        {
            if (i == start)
            {
                fprintf(stderr, "syntax error: empty command in pipeline\n");
                return 0;
            }

            char *segment[MAX_TOKENS] = {0};
            int idx = 0;

            for (int j = start; j < i && idx < MAX_TOKENS - 1; j++)
            {
                segment[idx++] = tokens[j];
            }

            segment[idx] = NULL;

            if (!validate_single_command(segment))
            {
                return 0;
            }

            if (tokens[i] == NULL)
            {
                break;
            }

            start = i + 1;
        }
    }

    return 1;
}

void handle_redirection(char *tokens[])
{
    for (int i = 0; tokens[i] != NULL; i++)
    {
        
        // INPUT <
        if (strcmp(tokens[i], "<") == 0)
        {
            if (tokens[i + 1] == NULL)
            {
                fprintf(stderr, "Missing file for redirection\n");
                exit(1);
            }

            int fd = open(tokens[i + 1], O_RDONLY);

            if (fd < 0)
            {
                perror("input file");
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1)
            {
                perror("dup2 input");
                close(fd);
                exit(1);
            }
            close(fd);
            tokens[i] = NULL;
            tokens[i + 1] = NULL;
        }

        // OUTPUT >
        else if (strcmp(tokens[i], ">") == 0)
        {
            if (tokens[i + 1] == NULL)
            {
                fprintf(stderr, "Missing file for redirection\n");
                exit(1);
            }

            int fd = open(tokens[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);

            if (fd < 0)
            {
                perror("output file");
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1)
            {
                perror("dup2 output");
                close(fd);
                exit(1);
            }
            close(fd);
            tokens[i] = NULL;
            tokens[i + 1] = NULL;
        }
    }
}

int split_pipeline(char *tokens[], char *commands[][MAX_TOKENS])
{
    /*
        splits one token list into separate commands based on |
    */
    int cmd_idx = 0;
    int token_idx = 0;

    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            commands[cmd_idx][token_idx] = NULL;
            cmd_idx++;
            token_idx = 0;
        }
        else
        {
            commands[cmd_idx][token_idx++] = tokens[i];
        }
    }

    commands[cmd_idx][token_idx] = NULL;
    return cmd_idx + 1;
}

static void execute_pipeline(char *tokens[], int background)
{
    /*
        runs commands connected with pipes

        example:
        ls | grep txt | wc
    */
    char *commands[MAX_TOKENS][MAX_TOKENS] = {{0}};
    pid_t pids[MAX_TOKENS] = {0};

    int num_cmds = split_pipeline(tokens, commands);

    int pipes[MAX_TOKENS][2];

    // make the pipes first
    for (int i = 0; i < num_cmds - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe failed");
            return;
        }
    }

    for (int i = 0; i < num_cmds; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork failed");
            return;
        }

        if (pid == 0)
        {
            // CHILD

            // if this is not the first command, get input from the previous pipe
            if (i > 0)
            {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                {
                    perror("dup2 pipe input");
                    exit(1);
                }
            }

            // if this is not the last command, send output into the next pipe
            if (i < num_cmds - 1)
            {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                {
                    perror("dup2 pipe output");
                    exit(1);
                }
            }

            // close all pipe ends in the child after dup2
            for (int j = 0; j < num_cmds - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // redirection still needs to work inside piped commands
            handle_redirection(commands[i]);

            // reset signal handlers to default so child behaves normally - Boris Hernandez
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            execvp(commands[i][0], commands[i]);
            perror("exec failed");
            exit(1);
        }

        pids[i] = pid;
    }

    // PARENT closes all pipes
    for (int i = 0; i < num_cmds - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (background)
    {
        // if it is a background pipeline, do not wait here
        printf("[background pid %d]\n", pids[num_cmds - 1]);
        return;
    }

    // Wait for all children

    current_child_pid = pids[num_cmds - 1];
    for (int i = 0; i < num_cmds; i++) {
        int status = 0;
        waitpid(pids[i], &status, WUNTRACED);
        if (i == num_cmds - 1) {
            handle_status(status);
        }
    }
    current_child_pid = -1;
}

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
        fflush(stdout);
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
        errno = 0;

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
    (void)sig;

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
    (void)sig;

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
    int saved_errno = errno;
    pid_t pid;
    int status;

    (void)sig;

    // keep reaping while there are finished children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (pid == current_child_pid)
        {
            current_child_pid = -1;
        }
    }

    errno = saved_errno;
}

void setup_signal_handlers()
{
    /*
        Sets up signal handlers for SIGINT, SIGTSTP, and SIGCHLD
        using sigaction() with proper error checking.
    */
    struct sigaction sa_int, sa_tstp, sa_chld;

    memset(&sa_int, 0, sizeof(sa_int));
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    memset(&sa_chld, 0, sizeof(sa_chld));

    // Setup SIGINT handler
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sa_int.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("Error setting up SIGINT handler");
        exit(1);
    }

    // Setup SIGTSTP handler
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sa_tstp.sa_handler = sigtstp_handler;

    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1)
    {
        perror("Error setting up SIGTSTP handler");
        exit(1);
    }

    // this one is for cleaning up child processes in the background
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sa_chld.sa_handler = sigchld_handler;

    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
    {
        perror("Error setting up SIGCHLD handler");
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
    else if (WIFSTOPPED(status))
    {
        signal = WSTOPSIG(status);
        fprintf(stderr, "\nChild stopped by signal: %d\n", signal);
    }
    else
    {
        printf("Child terminated abnormally\n");
    }
}

static void print_help(void)
{
    /*
        built in help command
        just prints the commands and features supported by the shell
    */
    printf("Simple Shell Help\n");
    printf("Built-in commands:\n");
    printf("  cd <dir>   Change current directory\n");
    printf("  help       Show this help message\n");
    printf("  exit       Exit the shell\n");
    printf("Features:\n");
    printf("  command execution\n");
    printf("  input redirection with <\n");
    printf("  output redirection with >\n");
    printf("  piping with |\n");
    printf("  background execution with &\n");
    printf("  Ctrl+C and Ctrl+Z signal forwarding\n");
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
    int background = 0;

    // special cases
    if (tokens[0] == NULL)
    {
        // if the user just hit space, do nothing
        return;
    }

    if (!validate_tokens(tokens))
    {
        return;
    }

    // check if this should run in the background
    background = is_background_command(tokens);
    if (background)
    {
        strip_background_token(tokens);

        if (tokens[0] == NULL)
        {
            fprintf(stderr, "syntax error: missing command before '&'\n");
            return;
        }
    }

    if (strcmp(tokens[0], "cd") == 0)
    {
        // changing directories affects the main shell process
        // so do not call fork for this

        if (tokens[1] == NULL)
        {
            char *home = getenv("HOME");
            if (home == NULL)
            {
                fprintf(stderr, "missing directory path argument\n");
                return;
            }

            if (chdir(home) == -1)
            {
                perror("failed to change directories");
            }
            return;
        }

        if (tokens[2] != NULL)
        {
            fprintf(stderr, "cd: too many arguments\n");
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
    else if (strcmp(tokens[0], "help") == 0)
    {
        // help is built in too, so no fork needed
        print_help();
        return;
    }
    else if (strcmp(tokens[0], "exit") == 0)
    {
        // exit is a built in that doesn't require fork
        printf("Thank you for using the shell!\n");
        exit(0);
    }

    // HANDLE PIPING FIRST (before fork)
    if (has_pipe(tokens)) {
    execute_pipeline(tokens, background);
        return;
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
      
        // HANDLE REDIRECTION IN CHILD
        if (has_redirection(tokens)) {
            handle_redirection(tokens);
        }

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
        if (background)
        {
            // do not wait if it is a background command
            printf("[background pid %d]\n", child_pid);
            return;
        }

        current_child_pid = child_pid;
        waitpid(child_pid, &status, WUNTRACED);
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
        memset(tokens, 0, sizeof(tokens));
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

    setup_signal_handlers();

    while (fgets(input, sizeof(input), fp) != NULL)
    {
        // strip both \n and \r (handles Windows CRLF line endings)
        input[strcspn(input, "\r\n")] = '\0';

        // skip empty lines and comment lines starting with #
        if (input[0] == '\0' || input[0] == '#')
            continue;

        // echo the command so you can see what's being run
        printf("%s$ %s\n", filename, input);

        memset(tokens, 0, sizeof(tokens));
        tokenize_input(input, tokens);
        run_command(tokens);
    }

    fclose(fp);
    printf("\nFile execution complete.\n");
}