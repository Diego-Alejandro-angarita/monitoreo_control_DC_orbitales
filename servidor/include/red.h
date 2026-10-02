#ifndef RED_H
#define RED_H

#include <stddef.h>
#include <sys/socket.h>

#define RED_MAX_ORIGEN 80

int red_crear_servidor(int puerto, int tipo);
int red_recv_exacto(int fd, void *buf, size_t n);
int red_send_todo(int fd, const void *buf, size_t n);
void red_origen(const struct sockaddr *sa, socklen_t len, char *origen);

#endif
