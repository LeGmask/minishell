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

sigset_t getMask_SIGINT_SIGTSTP(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    return mask;
}

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

void handleRedirects(struct cmdline *pCmdline) {
    if (pCmdline->in) {
        openAndDup(pCmdline->in, O_RDONLY, 0644, STDIN_FILENO);
    }

    if (pCmdline->out) {
        openAndDup(pCmdline->out, O_WRONLY | O_TRUNC | O_CREAT, 0644, STDOUT_FILENO);
    }
}

/**
 * Process and run a commend in a forked process
 *
 * @param cmd
 * @param backgrounded
 */
void handleCmd(char **cmd, struct cmdline *command) {
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
            handleRedirects(command);

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


int main(void) {
    bool fini = false;

    // Setup SIGCHLD handler
    set_signal(SIGCHLD, (__sighandler_t) child_handler);

    // Ignore SIGINT and SIGTSTP
//    set_signal(SIGINT, SIG_IGN);
//    set_signal(SIGTSTP, SIG_IGN);

    // Mask SIGINT and SIGTSTP
    setup_Mask_SIGINT_SIGTSTP();

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
                    if (cmd[0]) {
                        if (strcmp(cmd[0], "exit") == 0) {
                            fini = true;
                            printf("Au revoir ...\n");
                        } else {
                            handleCmd(cmd, commande);
                            printf("\n");
                        }

                        indexseq++;
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
