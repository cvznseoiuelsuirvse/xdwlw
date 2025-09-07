#ifndef XDWLW_IPC_H
#define XDWLW_IPC_H

int ipc_client_connect();
struct ipc_message *ipc_client_recv(int sfd);
int ipc_client_send(int sfd, struct ipc_message *msg);

int ipc_server_start();
struct ipc_message *ipc_server_listen(int sfd, int *cfd);
int ipc_server_send(int cfd, struct ipc_message *msg);
int ipc_server_close(int fd);

void ipc_message_free(struct ipc_message *msg);

#endif
