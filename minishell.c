#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "readcmd.h"
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>

/**
 * Process and run a commend in a forked process
 *
 * @param cmd
 * @param backgrounded
 */
void handleCmd(char **cmd, bool backgrounded) {
    pid_t pid_fork;
    pid_fork = fork();

    if (pid_fork == -1) {
        printf("echec du fork");
    } else if (pid_fork == 0) { // Fils
        execvp(cmd[0], cmd);
        exit(EXIT_FAILURE);
    } else { // père
        if (!backgrounded) {
            int cmdStatus;
            wait(&cmdStatus); // attente de la fin de la commande
        }
    }
}

int main(void) {
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

                /* Pour le moment le programme ne fait qu'afficher les commandes 
                   tapees et les affiche à l'écran. 
                   Cette partie est à modifier pour considérer l'exécution de ces
                   commandes 
                */
                int indexseq = 0;
                char **cmd;
                while ((cmd = commande->seq[indexseq])) {
                    if (cmd[0]) {
                        if (strcmp(cmd[0], "exit") == 0) {
                            fini = true;
                            printf("Au revoir ...\n");
                        } else {
                            handleCmd(cmd, commande->backgrounded);
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
