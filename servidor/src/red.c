#include "red.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int crear(int familia, const char *servicio, int tipo)
{
    struct addrinfo pistas, *lista;
    memset(&pistas, 0, sizeof pistas);
    pistas.ai_family = familia;
    pistas.ai_socktype = tipo;
    pistas.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, servicio, &pistas, &lista) != 0)
        return -1;

    int fd = -1;
    for (struct addrinfo *ai = lista; ai != NULL && fd < 0; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        int si = 1, no = 0;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si);
        if (ai->ai_family == AF_INET6)
            setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof no);
        if (bind(fd, ai->ai_addr, ai->ai_addrlen) < 0
            || (tipo == SOCK_STREAM && listen(fd, SOMAXCONN) < 0)) {
            close(fd);
            fd = -1;
        }
    }
    freeaddrinfo(lista);
    return fd;
}

int red_crear_servidor(int puerto, int tipo)
{
    char servicio[8];
    snprintf(servicio, sizeof servicio, "%d", puerto);
    int fd = crear(AF_INET6, servicio, tipo);
    if (fd < 0)
        fd = crear(AF_INET, servicio, tipo);
    return fd;
}

int red_recv_exacto(int fd, void *buf, size_t n)
{
    size_t leidos = 0;
    while (leidos < n) {
        ssize_t r = recv(fd, (char *)buf + leidos, n - leidos, 0);
        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
            return -1;
        if (r == 0)
            return leidos == 0 ? 0 : -1;
        leidos += (size_t)r;
    }
    return 1;
}

int red_send_todo(int fd, const void *buf, size_t n)
{
    size_t enviados = 0;
    while (enviados < n) {
        ssize_t r = send(fd, (const char *)buf + enviados, n - enviados, MSG_NOSIGNAL);
        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
            return -1;
        enviados += (size_t)r;
    }
    return 0;
}

void red_origen(const struct sockaddr *sa, socklen_t len, char *origen)
{
    char host[64], puerto[8];
    if (getnameinfo(sa, len, host, sizeof host, puerto, sizeof puerto,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        snprintf(origen, RED_MAX_ORIGEN, "desconocido");
        return;
    }
    const char *ip = strncmp(host, "::ffff:", 7) == 0 ? host + 7 : host;
    if (strchr(ip, ':') != NULL)
        snprintf(origen, RED_MAX_ORIGEN, "[%s]:%s", ip, puerto);
    else
        snprintf(origen, RED_MAX_ORIGEN, "%s:%s", ip, puerto);
}
