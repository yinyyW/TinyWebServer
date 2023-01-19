#include "../include/http_conn.h"
#include "../include/locker.h"
#include "../include/threadpool.h"
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);

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

void show_error(int connfd, const char* info) {
    std::cout << info << std::endl;
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        std::cout << "Usage: " << basename(argv[0]) << " ip_address port_number.\n";
        return 1;
    }
    
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    addsig(SIGPIPE, SIG_IGN);
    
    // create threadpool
    threadpool<http_conn>* pool = NULL;
    try {
        pool = new threadpool<http_conn>();
    } catch (...) {
        return 1;
    }
    
    // init users
    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;
    
    // listen socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);
    struct linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    
    // epoll events
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;
    
    std::cout << "start server.\n";
    while (true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            std::cout << "epoll failure.\n";
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address,
                            &client_addrlength);
                if (connfd < 0) {
                    std::cout << "errno is " << errno << std::endl;
                    continue;
                }
                if (http_conn::m_user_count > MAX_FD) {
                    show_error(connfd, "Internal server busy");
                }
                users[connfd].init(connfd, client_address);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockfd].close_conn();
            } else if (events[i].events & EPOLLIN) {
                if (users[sockfd].read()) {
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
        
    }
    
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}