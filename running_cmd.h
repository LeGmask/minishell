#ifndef MINISHELL_RUNNING_CMD_H
#define MINISHELL_RUNNING_CMD_H

#include <sys/types.h>
#include "readcmd.h"

enum cmd_status {
    RUNNING,
    SUSPENDED,
};

typedef enum cmd_status cmd_status_t;

// type liste_noeud_t
struct running_cmd {
    pid_t pid;
    char *cmd;
    cmd_status_t status;
    struct running_cmd *next;
};

typedef struct running_cmd running_cmd_t;

void destroy_running_cmds(running_cmd_t *cmd);

void add_running_cmd(running_cmd_t **head, pid_t pid, char *cmd);

void set_running_cmd_status(running_cmd_t *head, pid_t pid, cmd_status_t status);

void remove_running_cmd(running_cmd_t **head, pid_t pid);

running_cmd_t *find_running_cmd(running_cmd_t *head, pid_t pid);

void print_running_cmds(running_cmd_t *head);

#endif //MINISHELL_RUNNING_CMD_H
