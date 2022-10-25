// Implement your shell in this source file.
#define _SVID_SOURCE
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
#include "tokens.h"

#define BUFFER_SIZE 255

pid_t child_processes[UINT_MAX];
int child_count = 0;

//original std in and out to restore after redirection/piping
int og_in;
int og_out;

//keeps track of the last command executed for 'prev'
char last[BUFFER_SIZE] = "";

//code for 'exit'
void exit_help(char *my_argv[]) {
    //kill children so the whole thing exists
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

    //forking so that we can return if file doesn't exist
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
    //only execute if there is a previous cmd
    if (strcmp("", last) != 1) {
        char char_arr[BUFFER_SIZE];
        strcpy(char_arr, last);
        handle_command(char_arr);
    }
}

//code for 'help'
void help_help(char *my_argv[]) {
    write(1, "cd: This command should change the current working directory of the shell.\nsource: Execute a script.Takes a filename as an argument and processes each line of the file as a command, including built-ins.\nprev: Prints the previous command line and executes it again.\nhelp: Explains all the built-in commands available in your shell\n", 330);
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

//remove leading spaces and trailing in a string
char *removeLeadingAndTrailingSpaces(char *str){
    size_t size;
    char *end;
    size = strlen(str);

    //if size is zero
    if (!size)
        return str;

    //start counting spaces from back
    end = str + size - 1;
    while (end >= str && isspace(*end))
        end--;
    //put a end of string flag once spaces stop
    *(end + 1) = '\0';

    //move string pointer forward until no more spaces
    while (*str && isspace(*str))
        str++;

    return str;
}

// splits string in input on the specified delimiter and puts the split string in output, returns number of tokens
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

    //splits the commands by pipe order
    int num_subcommands = split(cmd, subcommands, "|");

    //initialize all pipes
    int pipe_list[num_subcommands - 1][2];
    for (int i = 0; i < num_subcommands - 1; i ++) {
        assert(pipe(pipe_list[i]) == 0);
    }

    //execute commands split by piping
    for (int i = 0; i < num_subcommands; i += 1) {

        //if input redirect
        if(strstr(subcommands[i],"<")){
            char* tempRedirect[2];
            split(subcommands[i],tempRedirect,"<");
            close(0);
            tempRedirect[1] = removeLeadingSpaces(tempRedirect[1]);
            assert(open(removeLeadingSpaces(tempRedirect[1]), O_RDONLY) != -1);
            subcommands[i] = tempRedirect[0];
        }

        //if output redirect
        if(strstr(subcommands[i],">")){
            char* tempRedirect[2];
            split(subcommands[i],tempRedirect,">");
            close(1);
            tempRedirect[1] = removeLeadingSpaces(tempRedirect[1]);
            assert(open(tempRedirect[1], O_WRONLY | O_CREAT | O_TRUNC, 0644) != -1);
            subcommands[i] = tempRedirect[0];
        }

        //get tokens for each command, not split due to double quote tokenization
        char **my_argv = get_tokens(subcommands[i]);
        assert(my_argv != NULL);
        int len = sizeof(my_argv);
        my_argv[len] = NULL;

        //check for built-in function and it successfully executes exit
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
            //command not found: we only get here if execvp fails
            printf("%s: command not found\n", my_argv[0]);
            exit(1);
        }
        //in parent process
        else {
            wait(&child_status);
            //kill child process if not successful
            if (WEXITSTATUS(child_status) == 1) {
                return;
            }
            if (i < num_subcommands - 1) {
                close(pipe_list[i][1]);
            }
        }
        //free tokens at the end
        free_tokens(my_argv);
    }
}

// reads in the commands from the stdin and simulates a mini shell
int main(int argc, char **argv){

    //set the original std in and out to be able to restore later
    og_in = dup(STDIN_FILENO);
    og_out = dup(STDOUT_FILENO);
    char input[BUFFER_SIZE];
    printf("Welcome to mini-shell.\n");

    while(1) {

        printf("shell $");
        fgets(input, BUFFER_SIZE, stdin);

        //exit on ctrl + D
        if (feof(stdin)){
            printf("\nBye bye.\n");
            exit(0);
        }

        //remove trailing newline after hitting enter for a command
        input[strcspn(input, "\n")] = 0;
        char *commands[BUFFER_SIZE];

        // split the input into different commands by semicolon
        int num_commands = split(input, commands, ";");

        //parse all commands
        for (int i = 0; i < num_commands; i += 1) {

            //executes the command
            handle_command(commands[i]);

            //restores regular std in and out after each command in case each command alters it
            close(0);
            close(1);
            dup2(og_in, STDIN_FILENO);
            dup2(og_out, STDOUT_FILENO);
        }
    }

    return 0;
}
