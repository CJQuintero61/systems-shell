/*
    shell.h
    created by Christian Quintero
    created on 03/22/2026

    This header file declares the functions for the shell program.
*/
#ifndef SHELL_H
#define SHELL_H

void print_prompt();
void print_cwd();
char* get_input(char* input, size_t size);
void tokenize_input(char input[], char* tokens[]);
void print_tokens(char* tokens[]);
void handle_status(int status);
void run_comamnd(char* tokens[]);
void run_shell();


#endif // SHELL_H