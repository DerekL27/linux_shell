// Implement your shell in this source file.

#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>

#define BUFFER_SIZE 255

pid_t child_processes[UINT_MAX];
int child_count = 0;

//keeps track of the last command executed for 'prev'
char last[BUFFER_SIZE] = "";

//code for 'exit'
void exit_help(char *my_argv[]) {
    for (int i = 0; i < child_count; i += 1) {
        kill(child_processes[i], SIGKILL);
    }
    printf("Bye bye.\n");
    exit(0);
}

//code for 'cd'
void cd_help(char *my_argv[]) {
    pid_t pid = fork();

    child_processes[child_count] = pid;
    child_count +=1 ;

    if (pid == 0) {
        chdir(my_argv[1]);
    }
    else {
        wait(NULL);
    }
}

//code for 'source'
void source_help(char *my_argv[]) {
    char *name = my_argv[1];
    FILE *path = fopen(name, "r");

    //create a new process in case the file cannot be found and needs to exit back to mini-shell
    pid_t pid = fork();
    child_processes[child_count] = pid;
    child_count += 1;
    if (pid == 0) {
        if (path == NULL) {
            write(1, "File not found\n", 15);
            exit(1);
        }
        else {
            char line[BUFFER_SIZE];
            while (fgets(line, BUFFER_SIZE, path)) {
                char *pch = strtok(line, "\n");
                if (pch != NULL) {
                    handle_command(pch);
                }
            }
        }
        fclose(path);
    }
    else {
        wait(NULL);
    }
}

//code for 'prev'
void prev_help(char *my_argv[]) {
    if (strcmp("", last) != 1) {
        char char_arr[BUFFER_SIZE];
        strcpy(char_arr, last);
        handle_command(char_arr);
    }
}

//code for 'help'
void help_help(char *my_argv[]) {
    write(1, "cd: This command should change the current working directory of the shell.\nsource: Execute a script.Takes a filename as an argument and processes each line of the file as a command, including built-ins.\nprev: Prints the previous command line and executes it again.\nhelp: Explains all the built-in commands available in your shell", 300);
}

//handles running built in commands
int built_in_help(char *my_argv[]) {
    if (strcmp(my_argv[0], "exit") == 0) {
        exit_help(my_argv);
    }
    else if (strcmp(my_argv[0], "cd") == 0) {
        cd_help(my_argv);
    }
    else if (strcmp(my_argv[0], "source") == 0) {
        source_help(my_argv);
    }
    else if (strcmp(my_argv[0], "prev") == 0) {
        prev_help(my_argv);
    }
    else if (strcmp(my_argv[0], "help") == 0) {
        help_help(my_argv);
    }
    else {
        return 1;
    }
    return 0;
}

// splits string in input on the specified delimiter and puts the split string in output
int split(char input[], char* output[], char* delim) {

    char *token;
    int num_tokens = 0;

        token = strtok(input, delim);
        while (token != NULL) {
            output[num_tokens] = token;
            token = strtok (NULL, delim);
            num_tokens += 1;
        }
    return num_tokens;
}

//handles piping
// index is index of current pipe
// length is length of subcommands
// pipes is the array of pipes
void handle_piping(int index, int length, int pipes[][2]) {
    // if there aren't any pipes
    if (length == 1) {
        return;
    }
    //if pipe is the first pipe
    if (index == 0) {
        //close read end of pipe
        close(pipes[index][0]);
        // replace stdout with write end of pipe
        close(1);
        assert(dup(pipes[index][1]) == 1);
    }
    //if the pipe is not the first or last
    else if (index > 0 && index < length - 1) {
        //replace in with previous pipe read
        close(0);
        assert(dup(pipes[index - 1][0]) == 0);
        //replace out with pipe out
        close(1);
        assert(dup(pipes[index][1]) == 1);
    }
    //if the pipe is the last one
    else {
        //replace stdin with read from previous pipe
        close(0);
        assert(dup(pipes[index - 1][0]) == 0);
    }
}

// executes the given command
void handle_command(char cmd[]) {
    if (strcmp("prev", cmd) != 0) {
        strcpy(last, cmd);
    }
    char *subcommands[BUFFER_SIZE];

    //splits the commands by pipe
    int num_subcommands = split(cmd, subcommands, "|");

    //init list of pipes
    int pipe_list[num_subcommands - 1][2];
    for (int i = 0; i < num_subcommands - 1; i ++) {
        assert(pipe(pipe_list[i]) == 0);
    }

    //execute commands split by piping
    for (int i = 0; i < num_subcommands; i += 1) {
        // current command in the string of pipes
        //char *my_argv[BUFFER_SIZE];
        // get length of commands to pipe
        char **my_argv = get_tokens(subcommands[i]);
        assert(my_argv != NULL);
        int len = sizeof(my_argv);
        my_argv[len] = NULL;

//        int len = split(subcommands[i], my_argv, " ");
//        my_argv[len] = NULL;
        if (built_in_help(my_argv) == 0) {
            return;
        }

        int child_status;

        pid_t pid = fork();
        child_processes[child_count] = pid;
        child_count +=1 ;
        //in child process
        if (pid == 0) {
            handle_piping(i, num_subcommands, pipe_list);
            execvp(my_argv[0], my_argv);
            printf("%s: command not found\n", my_argv[0]);
            exit(1);
        }
        //in parent process
        else {
            wait(&child_status);
            //terminate if child process fails
            if (WEXITSTATUS(child_status) == 1) {
                return;
            }
            //
            if (i < num_subcommands - 1) {
                close(pipe_list[i][1]);
            }
        }
        free_tokens(my_argv);
    }
}

// reads in the commands from the stdin and simulates a mini shell
int main(int argc, char **argv){

    char input[BUFFER_SIZE];
    printf("Welcome to mini-shell.\n");

    while(1) {

        printf("shell $");
        fgets(input, BUFFER_SIZE, stdin);
        if (feof(stdin)){
            printf("\nBye bye.\n");
            exit(0);
        }
        input[strcspn(input, "\n")] = 0;
        char *commands[BUFFER_SIZE];
        // split the input into different commands by semicolon
        int num_commands = split(input, commands, ";");

        //parse all commands
        for (int i = 0; i < num_commands; i += 1) {
            handle_command(commands[i]);
        }
    }

    return 0;
}
