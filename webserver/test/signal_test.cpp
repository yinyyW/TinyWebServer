#include "../include/timer.h"
#include <cerrno>
#include <sys/socket.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define TIME_SLOT 2
#define MAX_EVENT_NUMBER 1000
#define MAX_FD 500

static int pipefd[2];
static timer_lst tl;

void cb_func() {
    printf("alarm.\n");
}

void sig_handler(int sig) {
    printf("signal: %d\n", sig);
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

int main(int argc, char* argv[]) {
    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    // pipe
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);
    
    // add signal handler
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    
    // add timer
    timer* t1 = new timer();
    time_t cur = time(NULL);
    t1->expire = cur + 3 * TIME_SLOT;
    tl.add_timer(t1);
    
    // loop event
    bool stop = false;
    bool timeout = false;
    alarm(TIME_SLOT);
    printf("start.\n");
    while (!stop) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret < 0) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGTERM: {
                                printf("termination signal.\n");
                                stop = true;
                                break;
                            }
                            case SIGALRM: {
                                printf("timeout signal.\n");
                                timeout = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (timeout) {
            tl.tick();
            timeout = false;
        }
    }
    
    return 0;
}