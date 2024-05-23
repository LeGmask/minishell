#include "running_cmd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static const char* status_str[] = {
        "RUNNING",
        "SUSPENDED",
};

void destroy_running_cmds(running_cmd_t *cmd) {
    if (cmd == NULL) return;
    destroy_running_cmds(cmd->next);
    free(cmd);
}

void add_running_cmd(running_cmd_t **head, pid_t pid, char *cmd) {
    running_cmd_t *new_cmd = malloc(sizeof(running_cmd_t));
    new_cmd->pid = pid;
    new_cmd->cmd = strdup(cmd);
    new_cmd->next = *head;
    new_cmd->status = RUNNING;
    *head = new_cmd;
}

void set_running_cmd_status(running_cmd_t *head, pid_t pid, cmd_status_t status) {
    running_cmd_t *cmd = find_running_cmd(head, pid);
    if (cmd != NULL) {
        cmd->status = status;
    }
}

void remove_running_cmd(running_cmd_t **head, pid_t pid) {
    if (*head == NULL) return;
    if ((*head)->pid == pid) {
        running_cmd_t *tmp = *head;
        free(tmp->cmd);
        *head = (*head)->next;
        free(tmp);
        return;
    }
    remove_running_cmd(&(*head)->next, pid);
}

running_cmd_t *find_running_cmd(running_cmd_t *head, pid_t pid) {
    if (head == NULL) return NULL;
    if (head->pid == pid) {
        return head;
    }
    return find_running_cmd(head->next, pid);
}

void print_running_cmds(running_cmd_t *head) {
    if (head == NULL) return;
    printf("pid: %d, cmd: %s, status: %s\n", head->pid, head->cmd, status_str[head->status]);
    print_running_cmds(head->next);
}