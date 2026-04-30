#include "token.h"
#include "command.h"
#include "builtin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

static void killChild(int signo)
{
    pid_t pid;
    int status;

    (void) signo;

    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
            break;
    }
}

static void close_pipefds(int pipefds[], int pipeCount)
{
    for (int i = 0; i < pipeCount * 2; i++) {
        close(pipefds[i]);
    }
}

int execute_command(int input, int output, Command *cmd, int pipefds[], int pipeCount) {
    if (cmd->argv == NULL) {
        printf(" NO ARGS");
        return 0;
    }

    for (int i=0; i<getBuiltinCount(); i++) {
        if (strcmp(cmd->argv[0], builtin_cmd[i]) == 0) {
            return (*exec_builtin[i])(cmd);
        }
    }

    pid_t pid;
    int status;

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }
    else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        if (cmd->stdin_file != NULL) { //redirect input
            int fd0 = open(cmd->stdin_file, O_RDONLY, 0);
            if (fd0 < 0) {
                perror(cmd->stdin_file);
                exit(1);
            }
            dup2(fd0, STDIN_FILENO);
            close(fd0);
        }

        if (cmd->stdout_file != NULL) { //redirect ouput
            int fd1 = open(cmd->stdout_file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
            if (fd1 < 0) {
                perror(cmd->stdout_file);
                exit(1);
            }
            dup2(fd1, STDOUT_FILENO);
            close(fd1);
        }

        if (input != -1)
            dup2 (input, 0);

        if (output != -1)
            dup2 (output, 1);

        close_pipefds(pipefds, pipeCount);

        execvp(cmd->argv[0], cmd->argv);

        perror(cmd->argv[0]);
        exit(1);
    } else {
        if (strcmp(cmd->sep, conSep) != 0) {
            wait(&status);
        }

        if (input != -1)
            close (input);

        if (output != -1)
            close (output);

        return 0;
    }
}

char *prompt;

int main(void)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, killChild);


    if (!(prompt = malloc(256 * sizeof(char))))
        return 1;
    strcpy(prompt, "%");

    char *str = NULL;
    size_t len = 0;
    ssize_t nread;
    int status = 0;
    do {
       printf("\033[0;33m");
       printf("%s ", prompt);
       printf("\033[0m");
       fflush(stdout);

       int again = 1;
       while (again) {
            again = 0;
            nread = getline(&str, &len, stdin);
            if (nread < 0) {
                if (errno == EINTR)
                     again = 1;        // signal interruption, read again
                else {
                    printf("\n");
                    free(str);
                    free(prompt);
                    return 0;
                }
            }
       }

        if (nread > 0 && str[nread - 1] == '\n')
            str[nread - 1] = '\0';

        char **token = calloc(1000, sizeof(char*));
        if (token == NULL) {
            perror("calloc");
            free(str);
            free(prompt);
            return 1;
        }

        tokenise(str, token);

        Command cmds[MAX_NUM_COMMANDS];
        for (int i=0; i<MAX_NUM_COMMANDS; i++)
        {
            cmds[i].first = 0;
            cmds[i].last = 0;
            cmds[i].sep = NULL;
            cmds[i].argv = NULL;
            cmds[i].stdin_file = NULL;
            cmds[i].stdout_file = NULL;
        }

        int num = separateCommands(token, cmds);
        if (num < 0) {
            fprintf(stderr, "Invalid command syntax\n");
            free(token);
            continue;
        }

        //get pipe count
        int pipeCount = 0;
        for (int x=0; x<num; x++) {
            if (strcmp(cmds[x].sep, pipeSep) == 0) {
                pipeCount += 1;
            }
        }

        //initialise pipes
        int pipefds[2*pipeCount];
        for(int i = 0; i < pipeCount; i++ ){
            if( pipe(pipefds + i*2) < 0 ){
                perror("pipe");
                free(token);
                continue;
            }
        }

        int m = 0;
        for (int i=0; i<num; i++) {
            int in = -1;
            int out = -1;
            if (i != 0) { //input
                if (strcmp(cmds[i-1].sep, pipeSep) == 0) {
                    in = pipefds[m]; // 0
                    m+=2;
                }
            }

            if (strcmp(cmds[i].sep, pipeSep) == 0) { //output
                out = pipefds[m+1]; // 1
            }

            status = execute_command(in, out, &cmds[i], pipefds, pipeCount);
        }
        close_pipefds(pipefds, pipeCount);
        free(token);
    } while (!status);

    free(str);
    free(prompt);
    return(0);
}
