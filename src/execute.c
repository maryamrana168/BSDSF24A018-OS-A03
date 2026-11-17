\
#include "shell.h"
#include <fcntl.h>
#include <ctype.h>

/* =========================================
   BACKGROUND JOB MANAGEMENT
   ========================================= */

BackgroundJob jobs[MAX_JOBS];
int job_count = 0;

void add_job(pid_t pid, const char* command) {

    if (job_count >= MAX_JOBS) {
        printf("Maximum background jobs reached.\n");
        return;
    }

    jobs[job_count].pid = pid;

    strncpy(jobs[job_count].command, command, MAX_LEN - 1);
    jobs[job_count].command[MAX_LEN - 1] = '\0';

    job_count++;

    printf("[%d] Started: %s\n", pid, command);
}

void remove_job(pid_t pid) {

    for (int i = 0; i < job_count; i++) {

        if (jobs[i].pid == pid) {

            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }

            job_count--;
            return;
        }
    }
}

void display_jobs(void) {

    if (job_count == 0) {
        printf("No background jobs running.\n");
        return;
    }

    printf("Background Jobs:\n");

    for (int i = 0; i < job_count; i++) {
        printf("[%d] %s\n", jobs[i].pid, jobs[i].command);
    }
}

void reap_background_jobs(void) {

    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        printf("\n[%d] Background process completed.\n", pid);

        remove_job(pid);
    }
}

/* =========================================
   SHELL VARIABLES USING LINKED LIST
   ========================================= */

typedef struct ShellVariable {
    char* name;
    char* value;
    struct ShellVariable* next;
} ShellVariable;

static ShellVariable* variables = NULL;

/* Find an existing variable */
static ShellVariable* find_variable(const char* name) {

    ShellVariable* current = variables;

    while (current != NULL) {

        if (strcmp(current->name, name) == 0) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

/* Check whether a token is a valid assignment */
int is_variable_assignment(const char* token) {

    if (token == NULL) {
        return 0;
    }

    const char* equal = strchr(token, '=');

    if (equal == NULL || equal == token) {
        return 0;
    }

    if (!(isalpha((unsigned char)token[0]) || token[0] == '_')) {
        return 0;
    }

    for (const char* p = token; p < equal; p++) {

        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return 0;
        }
    }

    return 1;
}

/* Create a new variable or update an existing one */
int set_shell_variable(const char* name, const char* value) {

    if (name == NULL || value == NULL) {
        return 0;
    }

    ShellVariable* current = find_variable(name);

    if (current != NULL) {

        char* new_value = strdup(value);

        if (new_value == NULL) {
            perror("strdup");
            return 0;
        }

        free(current->value);
        current->value = new_value;

        return 1;
    }

    ShellVariable* new_variable = malloc(sizeof(ShellVariable));

    if (new_variable == NULL) {
        perror("malloc");
        return 0;
    }

    new_variable->name = strdup(name);
    new_variable->value = strdup(value);

    if (new_variable->name == NULL ||
        new_variable->value == NULL) {

        perror("strdup");

        free(new_variable->name);
        free(new_variable->value);
        free(new_variable);

        return 0;
    }

    new_variable->next = variables;
    variables = new_variable;

    return 1;
}

/* Display all stored variables */
void print_shell_variables(void) {

    ShellVariable* current = variables;

    if (current == NULL) {
        printf("No shell variables defined.\n");
        return;
    }

    while (current != NULL) {

        printf("%s=%s\n", current->name, current->value);

        current = current->next;
    }
}

/* Expand $VARNAME into its stored value */
char* expand_shell_variables(const char* token) {

    if (token == NULL) {
        return NULL;
    }

    size_t capacity = strlen(token) + 1;
    size_t length = 0;

    char* result = malloc(capacity);

    if (result == NULL) {
        perror("malloc");
        return NULL;
    }

    size_t i = 0;

    while (token[i] != '\0') {

        if (token[i] == '$' &&
            (isalpha((unsigned char)token[i + 1]) ||
             token[i + 1] == '_')) {

            i++;

            char name[256];
            size_t name_length = 0;

            while (isalnum((unsigned char)token[i]) ||
                   token[i] == '_') {

                if (name_length < sizeof(name) - 1) {
                    name[name_length++] = token[i];
                }

                i++;
            }

            name[name_length] = '\0';

            ShellVariable* variable = find_variable(name);

            const char* value =
                (variable != NULL) ? variable->value : "";

            size_t value_length = strlen(value);

            while (length + value_length + 1 > capacity) {

                capacity *= 2;

                char* temp = realloc(result, capacity);

                if (temp == NULL) {
                    perror("realloc");
                    free(result);
                    return NULL;
                }

                result = temp;
            }

            memcpy(result + length, value, value_length);
            length += value_length;
        }
        else {

            if (length + 2 > capacity) {

                capacity *= 2;

                char* temp = realloc(result, capacity);

                if (temp == NULL) {
                    perror("realloc");
                    free(result);
                    return NULL;
                }

                result = temp;
            }

            result[length++] = token[i++];
        }
    }

    result[length] = '\0';

    return result;
}

/* =========================================
   BUILT-IN COMMANDS
   ========================================= */

/*
   Returns:
   0  = not a built-in
   1  = built-in handled
  -1  = exit command
*/

int handle_builtin(char** arglist) {

    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        return -1;
    }

    if (strcmp(arglist[0], "cd") == 0) {

        if (arglist[1] == NULL) {
            printf("Usage: cd <directory>\n");
        }
        else if (chdir(arglist[1]) == -1) {
            perror("cd");
        }

        return 1;
    }


    if (strcmp(arglist[0], "help") == 0) {

        printf("Built-in Commands:\n");
        printf("exit    - Exit the shell\n");
        printf("cd      - Change directory\n");
        printf("help    - Display available commands\n");
        printf("jobs    - Display background jobs\n");
        printf("history - Display command history\n");
        printf("set     - Display shell variables\n");

        printf("\nShell Variables:\n");
        printf("NAME=value - Assign a value to a variable\n");
        printf("set        - Display all shell variables\n");
        printf("echo $NAME - Display a variable's value\n");

        printf("\nExample:\n");
        printf("MSG=\"Hello World\"\n");
        printf("echo $MSG\n");

        return 1;
    }
    

    if (strcmp(arglist[0], "history") == 0) {
        display_history();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        display_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        print_shell_variables();
        return 1;
    }

    return 0;
}

/* =========================================
   PROCESS EXIT STATUS
   ========================================= */

static int get_exit_status(int status) {

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

/* =========================================
   EXECUTE ONE COMMAND
   ========================================= */

static int execute_single(char** args,
                          int input_fd,
                          int output_fd,
                          int background,
                          const char* command) {

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {

        if (input_fd != STDIN_FILENO) {

            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("dup2 input");
                exit(1);
            }

            close(input_fd);
        }

        if (output_fd != STDOUT_FILENO) {

            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                exit(1);
            }

            close(output_fd);
        }

        execvp(args[0], args);

        perror(args[0]);
        exit(1);
    }

    if (background) {
        add_job(pid, command);
        return 0;
    }

    int status;

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    return get_exit_status(status);
}

/* =========================================
   MAIN EXECUTION FUNCTION
   ========================================= */

int execute(char* arglist[], int background, const char* command) {

    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    /* Expand variables in command arguments */
    for (int i = 0; arglist[i] != NULL; i++) {

        char* expanded = expand_shell_variables(arglist[i]);

        if (expanded == NULL) {
            return 1;
        }

        free(arglist[i]);
        arglist[i] = expanded;
    }

    /* Handle variable assignment in the parent shell */
    if (is_variable_assignment(arglist[0]) &&
        arglist[1] == NULL) {

        char* equal = strchr(arglist[0], '=');

        size_t name_length = (size_t)(equal - arglist[0]);

        char* name = malloc(name_length + 1);

        if (name == NULL) {
            perror("malloc");
            return 1;
        }

        memcpy(name, arglist[0], name_length);
        name[name_length] = '\0';

        int result = set_shell_variable(name, equal + 1);

        free(name);

        return result ? 0 : 1;
    }

    /* Handle built-in commands */
    if (strcmp(arglist[0], "|") != 0 &&
        strcmp(arglist[0], "<") != 0 &&
        strcmp(arglist[0], ">") != 0) {

        int result = handle_builtin(arglist);

        if (result == -1) {
            return -1;
        }

        if (result == 1) {
            return 0;
        }
    }

    /* Prepare commands for pipes */
    char* commands[MAXARGS][MAXARGS];

    int command_count = 0;
    int arg_count = 0;

    char* input_file = NULL;
    char* output_file = NULL;

    for (int i = 0; arglist[i] != NULL; i++) {

        if (strcmp(arglist[i], "|") == 0) {

            if (arg_count == 0 || command_count >= MAXARGS - 1) {
                fprintf(stderr, "Syntax error near pipe\n");
                return 1;
            }

            commands[command_count][arg_count] = NULL;

            command_count++;
            arg_count = 0;
        }

        else if (strcmp(arglist[i], "<") == 0) {

            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Missing input filename\n");
                return 1;
            }

            input_file = arglist[++i];
        }

        else if (strcmp(arglist[i], ">") == 0) {

            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Missing output filename\n");
                return 1;
            }

            output_file = arglist[++i];
        }

        else {

            if (arg_count >= MAXARGS - 1) {
                fprintf(stderr, "Too many arguments\n");
                return 1;
            }

            commands[command_count][arg_count++] = arglist[i];
        }
    }

    if (arg_count == 0) {
        fprintf(stderr, "Syntax error: missing command\n");
        return 1;
    }

    commands[command_count][arg_count] = NULL;
    command_count++;

    int input_fd = STDIN_FILENO;
    int output_fd = STDOUT_FILENO;

    /* Open input file */
    if (input_file != NULL) {

        input_fd = open(input_file, O_RDONLY);

        if (input_fd == -1) {
            perror(input_file);
            return 1;
        }
    }

    /* Open output file */
    if (output_file != NULL) {

        output_fd = open(output_file,
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0644);

        if (output_fd == -1) {

            perror(output_file);

            if (input_fd != STDIN_FILENO) {
                close(input_fd);
            }

            return 1;
        }
    }

    int execution_status = 0;

    /* Execute a single command */
    if (command_count == 1) {

        execution_status = execute_single(
            commands[0],
            input_fd,
            output_fd,
            background,
            command
        );
    }

    /* Execute a pipeline */
    else {

        int previous_read = input_fd;

        pid_t pids[MAXARGS];

        for (int i = 0; i < command_count; i++) {

            int pipe_fd[2];

            if (i < command_count - 1) {

                if (pipe(pipe_fd) == -1) {
                    perror("pipe");

                    if (previous_read != STDIN_FILENO) {
                        close(previous_read);
                    }

                    if (output_fd != STDOUT_FILENO) {
                        close(output_fd);
                    }

                    return 1;
                }
            }

            int next_output =
                (i < command_count - 1) ? pipe_fd[1] : output_fd;

            pid_t pid = fork();

            if (pid == -1) {
                perror("fork");

                if (i < command_count - 1) {
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                }

                if (previous_read != STDIN_FILENO) {
                    close(previous_read);
                }

                if (output_fd != STDOUT_FILENO) {
                    close(output_fd);
                }

                return 1;
            }

            if (pid == 0) {

                if (previous_read != STDIN_FILENO) {
                    dup2(previous_read, STDIN_FILENO);
                }

                if (next_output != STDOUT_FILENO) {
                    dup2(next_output, STDOUT_FILENO);
                }

                if (previous_read != STDIN_FILENO) {
                    close(previous_read);
                }

                if (i < command_count - 1) {
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                }

                if (output_fd != STDOUT_FILENO) {
                    close(output_fd);
                }

                execvp(commands[i][0], commands[i]);

                perror(commands[i][0]);
                exit(1);
            }

            pids[i] = pid;

            /* Close previous input in parent */
            if (previous_read != STDIN_FILENO) {
                close(previous_read);

                if (previous_read == input_fd) {
                    input_fd = STDIN_FILENO;
                }
            }

            /* Prepare next pipeline input */
            if (i < command_count - 1) {
                close(pipe_fd[1]);
                previous_read = pipe_fd[0];
            }
        }

        if (previous_read != STDIN_FILENO) {
            close(previous_read);
        }

        if (output_fd != STDOUT_FILENO) {
            close(output_fd);
            output_fd = STDOUT_FILENO;
        }

        /* Background pipeline */
        if (background) {

            for (int i = 0; i < command_count; i++) {
                add_job(pids[i], command);
            }
        }

        /* Foreground pipeline */
        else {

            int last_status = 0;

            for (int i = 0; i < command_count; i++) {

                int status;

                if (waitpid(pids[i], &status, 0) == -1) {
                    perror("waitpid");
                    last_status = 1;
                }
                else if (i == command_count - 1) {
                    last_status = get_exit_status(status);
                }
            }

            execution_status = last_status;
        }
    }

    /* Close remaining file descriptors */
    if (input_fd != STDIN_FILENO) {
        close(input_fd);
    }

    if (output_fd != STDOUT_FILENO) {
        close(output_fd);
    }

    return execution_status;
}
