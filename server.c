#include "server.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct poll_handler;

static int request_handler(int sd, const struct poll_handler* ph);
static int pollin(int sd);
static int default_in(int sd);
static int default_hup(int sd);
static int default_err(int sd);
static int default_nval(int sd);
static int default_noev(int sd);
static void init_hadlers(struct poll_handler* ph, const struct poll_handler* cph);
static int error(int sd);

struct poll_handler {
    int (*in)(int sd);
    int (*hup)(int sd);
    int (*err)(int sd);
    int (*nval)(int sd);
    int (*noev)(int sd);
};

int init_server(unsigned short port)
{
    int sd;

    sd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sd == -1)
        return error(sd);

    struct sockaddr_un sun;
    socklen_t sun_len = sizeof(struct sockaddr_un);

    sun.sun_family = AF_UNIX;
    const char* spath = "/tmp/gitm.sock";
    memcpy(sun.sun_path, spath, strlen(spath));

    int status;
    status = bind(sd, (const struct sockaddr*)&sun, sun_len);

    if (status == -1)
        return error(sd);

    status = listen(sd, 10);

    if (status == -1)
        return error(sd);

    printf("Listening for connections...\n");

    status = request_handler(sd, NULL);

    if (status)
        return error(sd);

    close(sd);

    return 0;
}

int request_handler(int sd, const struct poll_handler* ph)
{
    int status, nfds = 0, i, revents = 0;
    struct poll_handler __ph;
    const struct poll_handler __cph = { pollin, NULL, NULL, NULL, NULL };
    struct pollfd pfd[2];
    struct sockaddr_in sa;
    socklen_t sa_len;

    init_hadlers(&__ph, &__cph);

    pfd[0].fd = sd;
    pfd[0].events = POLLIN;
    nfds++;

    while (1) {
        revents = 0;
        status = poll(pfd, nfds, 100);
        if (status == -1)
            return 1;

        if (pfd[0].revents == POLLIN) {
            pfd[nfds].fd = accept(sd, (struct sockaddr*)&sa, &sa_len);
            pfd[nfds].events = POLLIN;
#ifdef __DEBUG__
            printf("CONNECTION ACCEPTED\n");
#endif
            revents = 1;
            nfds++;
        } else if (nfds > 1) {
            for (i = 1; i < nfds; ++i) {
                switch (pfd[i].revents) {
                case POLLIN:
                    status = __ph.in(pfd[1].fd);
                    if (status) {
                        close(pfd[i].fd);
                        --nfds;
                    } else if (status == -1) {
                        close(pfd[i].fd);
                        return 1;
                    }
                    revents = 1;
                    break;
                case POLLHUP:
                    __ph.hup(sd);
                    revents = 1;
                    getchar();
                    break;
                case POLLERR:
                    __ph.err(sd);
                    revents = 1;
                    getchar();
                    break;
                case POLLNVAL:
                    __ph.nval(sd);
                    revents = 1;
                    getchar();
                    break;
                default:;
                }
            }
        }
        if (revents)
            continue;
        __ph.noev(sd);
    }

    return 0;
}

int pollin(int sd)
{
    unsigned char buff[1024];
    size_t buff_size = 1024 * sizeof(unsigned char);

    int rbytes = recv(sd, buff, buff_size, 0);

#ifdef __DEBUG__
    printf("RBYTES: %d\n", rbytes);
    perror("recv");
#endif
    if (rbytes == -1)
        return -1;

    if (rbytes == 0)
        return 1;

    for (int i = 0; i < rbytes; ++i)
        printf("%.2X ", buff[i]);

    return 0;
}

int default_in(int sd)
{
    printf("POLLIN from socket: %d\n", sd);
    return 1;
}

int default_hup(int sd)
{
    printf("POLLHUP from socket: %d\n", sd);
    return 1;
}

int default_err(int sd)
{
    printf("POLLERR from socket: %d\n", sd);
    return 1;
}

int default_nval(int sd)
{
    printf("POLLNVAL from socket: %d\n", sd);
    return 1;
}

int default_noev(int sd)
{
    printf("Waiting for events...\n");
    return 1;
}

void init_hadlers(struct poll_handler* ph, const struct poll_handler* cph)
{
    if (cph && cph->in)
        ph->in = cph->in;
    else
        ph->in = default_in;

    if (cph && cph->hup)
        ph->hup = cph->hup;
    else
        ph->hup = default_hup;

    if (cph && cph->err)
        ph->err = cph->err;
    else
        ph->err = default_err;

    if (cph && cph->nval)
        ph->nval = cph->nval;
    else
        ph->nval = default_nval;

    if (cph && cph->noev)
        ph->noev = cph->noev;
    else
        ph->noev = default_noev;
}

int error(int sd)
{
    close(sd);
    return 1;
}
