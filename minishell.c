#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "readcmd.h"
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

/**
 * Store the pid of the current foreground command
 */
int foreground_cmd = 0;

/**
 * Store the pipe file descriptors
 */
int pipefd[2];

/**
 * set sigaction for a signal
 * @param signal : signal to set
 * @param handler : handler to set
 */
int set_signal(int signal, void (*handler)(int)) {
    struct sigaction action;
    action.sa_handler = handler;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    return sigaction(signal, &action, NULL);
}

/**
 * Get the mask for SIGINT and SIGTSTP
 */
sigset_t getMask_SIGINT_SIGTSTP(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    return mask;
}

/**
 * Mask SIGINT and SIGTSTP
 */
void setup_Mask_SIGINT_SIGTSTP(void) {
    sigset_t toMask;
    toMask = getMask_SIGINT_SIGTSTP();
    sigprocmask(SIG_BLOCK, &toMask, NULL);
}

/**
 * Handle child process
 */
void child_handler(void) {
    int cmdStatus;
    pid_t pid_child;

    do {
        pid_child = waitpid(-1, &cmdStatus, WNOHANG | WUNTRACED | WCONTINUED);

        if (foreground_cmd == pid_child) {
            foreground_cmd = 0;
        }

        if (pid_child > 0) {
            if (WIFEXITED(cmdStatus)) {
                printf("Le processus %d s'est terminé normalement avec le code %d\n", pid_child,
                       WEXITSTATUS(cmdStatus));
            } else if (WIFSIGNALED(cmdStatus)) {
                printf("Le processus %d s'est terminé anormalement avec le code %d\n", pid_child, WTERMSIG(cmdStatus));
            } else if (WIFSTOPPED(cmdStatus)) {
                printf("Le processus %d a été stoppé par le signal %d\n", pid_child, WSTOPSIG(cmdStatus));
            } else if (WIFCONTINUED(cmdStatus)) {
                printf("Le processus %d a été relancé\n", pid_child);
            }
        }

    } while (pid_child > 0);
}

/**
 * Try to open a file and run dup2 with fd
 *
 * @param file the file to open
 * @param flags the flags to open the file
 * @param mode the mode to open the file
 * @param fd the file descriptor to dup2
 */
void openAndDup(char *file, int flags, int mode, int fd) {
    int file_fd = open(file, flags, mode);
    if (file_fd == -1) {
        char msg[100];
        sprintf(msg, "Erreur ouverture fichier: \"%s\"\n", file);
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    if (dup2(file_fd, fd) == -1) {
        write(STDERR_FILENO, "Erreur dup2\n", 12);
        exit(EXIT_FAILURE);
    }

    close(file_fd);
}


/**
 * Run dup2 with oldfd and newfd and close oldfd
 *
 * @param oldfd old file descriptor
 * @param newfd new file descriptor
 */
void dup2AndClose(int oldfd, int newfd) {
    if (dup2(oldfd, newfd) == -1) {
        write(STDERR_FILENO, "Erreur dup2\n", 12);
        exit(EXIT_FAILURE);
    }

    close(oldfd);
}

/**
 * Handle input and output redirections
 *
 * @param pCmdline the command line
 * @param pipeIn the input pipe
 * @param pipeOut the output pipe
 */
void handleRedirects(struct cmdline *pCmdline, int pipeIn, int pipeOut) {
    if (pipeIn != -1) {
        dup2AndClose(pipeIn, STDIN_FILENO);
    } else if (pCmdline->in) {
        openAndDup(pCmdline->in, O_RDONLY, 0644, STDIN_FILENO);
    }

    if (pipeOut != -1) {
        dup2AndClose(pipeOut, STDOUT_FILENO);
    } else if (pCmdline->out) {
        openAndDup(pCmdline->out, O_WRONLY | O_TRUNC | O_CREAT, 0644, STDOUT_FILENO);
    }
}

/**
 * Process and run a command in a forked process
 *
 * @param cmd
 * @param backgrounded
 */
void handleCmd(char **cmd, struct cmdline *command, int pipeIn, int pipeOut) {
    sigset_t toUnMask; // to unmask signals
    pid_t pid_fork;
    pid_fork = fork();

    switch (pid_fork) {
        case -1:
            write(STDERR_FILENO, "Erreur fork\n", 12);
            break;

        case 0: // Fils
            // UnMask all signals
            sigprocmask(SIG_BLOCK, NULL, &toUnMask);
            sigprocmask(SIG_UNBLOCK, &toUnMask, NULL);

            if (command->backgrounded) {
                // the program is in background -> detach from the terminal process group
                setpgid(0, 0);
            }

//          if (!backgrounded) {
//                // the program is in foreground -> unMask SIGINT and SIGTSTP
//                // Solution 2.
//              sigset_t toUnMask;
//              toUnMask = getMask_SIGINT_SIGTSTP();
//              sigprocmask(SIG_UNBLOCK, &toUnMask, NULL);

//                // UnIgnore SIGINT and SIGTSTP
//                // Solution 1.
//              set_signal(SIGINT, SIG_DFL);
//              set_signal(SIGTSTP, SIG_DFL);
//          }

            // Redirect input and output
            handleRedirects(command, pipeIn, pipeOut);

            execvp(cmd[0], cmd);

            // Solution 1.
//          set_signal(SIGINT, SIG_IGN);
//          set_signal(SIGTSTP, SIG_IGN);
            printf("erreur lors de l'éxécution de la command : %s\n", *cmd);
            exit(EXIT_FAILURE);

        default: // père
            if (!command->backgrounded) {
                foreground_cmd = pid_fork;
                while (foreground_cmd > 0) {
                    pause();
                }
            }
            break;
    }
}

/**
 * Handle sequence of commands (and piped if more than one)
 *
 * @param command   the command to handle
 * @param indexseq  index of the command in the sequence
 * @param isLastCmd boolean to know if the command is the last one
 */
void handlePipedCmd(struct cmdline *command, int indexseq, bool isLastCmd) {
    int pipeIn = -1; // disable input pipe
    int pipeOut = -1; // disable output pipe

    if (indexseq > 0) {
        pipeIn = pipefd[0];
    }

    if (!isLastCmd) {
        pipe(pipefd);
        pipeOut = pipefd[1];
    }

    handleCmd(command->seq[indexseq], command, pipeIn, pipeOut);
    close(pipeIn); // close the input pipe used
    close(pipeOut); // close the output pipe used
}

/**
 * Main loop that handle the prompt
 */
void promptLoop(void) {
    bool fini = false;
    while (!fini) {
        printf("> ");
        struct cmdline *commande = readcmd();

        if (commande == NULL) {
            // commande == NULL -> erreur readcmd()
            perror("erreur lecture commande \n");
            exit(EXIT_FAILURE);

        } else {
            if (commande->err) {
                // commande->err != NULL -> commande->seq == NULL
                printf("erreur saisie de la commande : %s\n", commande->err);

            } else {
                int indexseq = 0;
                char **cmd;

                while ((cmd = commande->seq[indexseq])) {
                    bool isLastCmd = commande->seq[indexseq + 1] == NULL;
                    if (cmd[0]) {
                        if (strcmp(cmd[0], "exit") == 0) {
                            fini = true;
                            printf("Au revoir ...\n");
                        } else {
                            handlePipedCmd(commande, indexseq, isLastCmd);
                            printf("\n");
                        }
                        indexseq++;
                    }
                }
            }
        }
    }
}


int main(void) {
    // Setup SIGCHLD handler
    set_signal(SIGCHLD, (__sighandler_t) child_handler);

    // Ignore SIGINT and SIGTSTP
//    set_signal(SIGINT, SIG_IGN);
//    set_signal(SIGTSTP, SIG_IGN);

    // Mask SIGINT and SIGTSTP
    setup_Mask_SIGINT_SIGTSTP();

    // Main loop that handle the prompt
    promptLoop();

    return EXIT_SUCCESS;
}
