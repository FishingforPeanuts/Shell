#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include "c-vector/cvector.h"
#include <stdio.h>
#include <unistd.h> 
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_BUF 300

/*
    1. -h <filename>
        - load history from given file, after exit: append history to file.
    2. -f <filename>
        - loads file and runs/prints commands, saves into history file.
    3. 
*/
char * cwd = NULL;
char * history_file_path = NULL;
char ** history_file = NULL;
char ** script_file = NULL;

typedef struct {
    pid_t id;
    int status;
} process;

typedef struct op_args {
    char ** argv1;
    char ** argv2;
} op_args;

void print_usage() {
    printf("-h <filename>: Supply a history file to store previous and future commands.\n");
    printf("-f <filename>: Supply a script file to execute commands from.");
}

void print_fork_fail() {
    printf("Fork attempt failed.\n");
}

void print_wait_fail() {
    printf("Wait for process failed.\n");
}

void print_exec_fail(pid_t id) {
    printf("Execution of process %lu failed.\n", id);
}

void print_read_fail(char * filename) {
    printf("File: \"%s\" cannot be found.", filename);
}

int execute_command(char *argv[]) {
    fflush(stdout);
    pid_t child_id = fork();
    if (child_id == -1) {
        print_fork_fail(); 
        exit(EXIT_FAILURE);
    } else if (child_id > 0) {
        int status;
        waitpid(child_id, &status, 0);
        if(WEXITSTATUS(status) != 0 || !WIFEXITED(status)) {
			print_wait_fail();
            return 1;
        }
    } else {
        pid_t id = getpid();
        execvp(argv[0], argv);
        print_exec_fail(id);
        exit(EXIT_FAILURE);
    }
    return 0;
}

char ** read_file(char * filename) {
    char ** ret = NULL;
    FILE * fp;
    char * temp = NULL;
    size_t len = 0;
    ssize_t read;
    char buf[PATH_MAX];
    char *path = realpath(filename, buf);
    fp = fopen(path, "r");
    if (fp == NULL) {
        print_read_fail(filename);
        exit(EXIT_FAILURE);
    }
    while ((read = getline(&temp, &len, fp)) != -1) {
        char * line = malloc(len);
        strcpy(line, temp);
        cvector_push_back(ret, line);
    }
    fclose(fp);
    return ret;
}

void parse_args(int argc, char *argv[]) {
    int opt;
    while((opt = getopt(argc, argv, "h:f:")) != -1) 
    { 
        switch(opt) 
        { 
            case 'h':
                history_file = read_file(optarg);
                char buf[PATH_MAX];
                history_file_path = realpath(optarg, buf);
                break;
            case 'f':
                script_file = read_file(optarg);
                break;
            default:
                print_usage();
                exit(1); 
        } 
    }
    for (int index = optind; index < argc; index++)
        if (argv[index]) {
            printf ("Non-option argument %s\n", argv[index]);
            print_usage();
            exit(1);
        }
}

char ** parse_input(char * input) {
    char ** ret = NULL;
    char * cpy = malloc(sizeof(input));
    strcpy(cpy, input);
    char * arg = strtok(cpy, " ");
    while (arg != NULL) {
        cvector_push_back(ret, arg);
        arg = strtok(NULL, " ");
    }
    return ret;
}

bool arr_contains(int size, char ** input, char * delim) {
        for (int i = 1; i < size; ++i) {
            if (strcmp(delim, input[i]) == 0) {
                return true;
            }
        }
        return false;
}

char * contains_op(int size, char ** input) {
    char * operators[] = {"&&", "||", ";;"};
    int nmb_operators = 3;
    for (int i = 0; i < nmb_operators; ++i) {
        if (arr_contains(size, input, operators[i])) {
            return operators[i];
        }
    }
    return NULL;
}

void change_directory(char * path) {
    if (path[0] == '/') {
        char buf[PATH_MAX];
        char * temp_path = realpath(path, buf);
        if (temp_path) {
            free(cwd);
            cwd = temp_path;
        }
    } else {
        char * cpy = malloc(strlen(cwd) + strlen(path) + 2);
        sprintf(cpy, "%s/%s", cwd, path);
        char buf[PATH_MAX];
        char * temp_path = realpath(cpy, buf);
        if (temp_path) {
            free(cwd);
            cwd = temp_path;
        }
        free(cpy);
    }
    
}

bool is_builtin(char ** args) {
    if (args) {
        if (args[0] == "cd") {
            change_directory(args[1]);
        }
    }
    return true;
}

struct op_args get_op_args(int size, char ** input, char* delim) {
    struct op_args args;
    char ** argv1 = NULL;
    char ** argv2 = NULL;
    bool delimSeen = false;
    for (int j = 0; j < size; ++j) {
        if (strcmp(delim, input[j]) == 0) {
            delimSeen = true;
            continue;
        } else if (delimSeen) {
            cvector_push_back(argv2, input[j]);
        } else {
            cvector_push_back(argv1, input[j]);
        }
    }
    cvector_push_back(argv1, NULL);
    cvector_push_back(argv2, NULL);
    args.argv1 = argv1;
    args.argv2 = argv2;
    return args;
}

void execute_op_expression(char * op, char** args) {
    struct op_args args2 = get_op_args(cvector_size(args), args, op);
    if (!strcmp(op, "&&")) {
        if (!execute_command(args2.argv1)) {
            execute_command(args2.argv2);
        }
    } else if (!strcmp(op, "||")) {
        if (execute_command(args2.argv1)) {
            execute_command(args2.argv2);
        }
    } else if (!strcmp(op, ";;")) {
        execute_command(args2.argv1);
        execute_command(args2.argv2);
    }
}

void print_history() {
    for (int i = 0; i < cvector_size(history_file); ++i) {
        printf("%lu    %s", i, history_file[i]);
    }
}


int main(int argc, char *argv[]) {
    cwd = get_current_dir_name();
    //is_builtin(test_args);
    printf("%s\n",cwd);
    char * input = "echo dfgdfg || ls";
    char ** args = parse_input(input);
    char * op = contains_op(cvector_size(args), args);
    if (op) {
        execute_op_expression(op, args);
    } else {
        //execute_expression(args);
    }
    parse_args(argc, argv);
    print_history();
    cvector_free(history_file);
    cvector_free(script_file);
    return 0;
}
