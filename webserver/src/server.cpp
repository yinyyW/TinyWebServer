#include "../include/server.h"
#include <bits/types/time_t.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

// ################### UTILS FUNCTIONS ###################
int Server::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Server::addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Server::sig_handler(int sig) {
    // printf("signal: %d\n", sig);
    int save_errno = errno;
    int msg = sig;
    send(m_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void Server::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Server::show_error(int connfd, const char* info) {
    printf("error: %s.\n", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void Server::cb_func(client_data* user_data) {
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    LOG_INFO("close fd %d", user_data->sockfd);
    --http_conn::m_user_count;
}

// ################### INITIALIZE STATIC VARIABLES ###################
int Server::m_epollfd = 0;
int Server::m_pipefd[2] = {0,0};

// ################### START SERVER ###################
Server::Server() {
    // init users
    users = new http_conn[MAX_FD];
    users_timer = new client_data[MAX_FD];
    m_timer_list = timer_lst();
    
    // server path
    char server_path[200];
    getcwd(server_path, 200);
    char root[10] = "/resource";
    m_doc_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_doc_root, server_path);
    strcat(m_doc_root, root);
}

Server::~Server() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);

    delete [] users;
    delete [] users_timer;
    delete m_thread_pool;
}

void Server::init_log() {
    if (m_close_log == 0) {
        if (m_async_log == 1) {
            Log::get_instance()->init("./server_log", 0, 2000, 
                                    800000, 800);
        } else {
            Log::get_instance()->init("./server_log", 0, 2000, 
                                    800000, 0);
        }
    }
}

void Server::init_thread_pool() {
    m_thread_pool = new threadpool<http_conn>(m_thread_num);
}

void Server::init_conn_pool() {
    m_conn_pool = connection_pool::getInstance();
    m_conn_pool->init("localhost", m_db_user, m_db_password, m_db_name,
                        3306, m_db_max_conn, m_close_log);
}

void Server::init(int port, int close_log, int async_log, int opt_linger,
                int thread_num, int db_max_conn,
                std::string database_name, std::string database_username,
                std::string database_password) {
    m_port = port;
    m_opt_linger = opt_linger;
    m_thread_num = thread_num;
    
    m_close_log = close_log;
    m_async_log = async_log;
    
    m_db_name = database_name;
    m_db_user = database_username;
    m_db_password = database_password;
    m_db_max_conn = db_max_conn;
    
    init_log();
    init_thread_pool();
    init_conn_pool();
}

void Server::init_new_conn(int connfd, struct sockaddr_in client_address) {
    users[connfd].init(connfd, client_address, m_doc_root, m_conn_pool);
    users_timer[connfd].sockfd = connfd;
    users_timer[connfd].client_addr = client_address;
    
    timer* t = new timer();
    t->user_data = &users_timer[connfd];
    time_t curr = time(NULL);
    t->expire = curr + 3 * TIMESLOT;
    t->cb_func = cb_func;
    users_timer[connfd].conn_timer = t;
    
    m_timer_list.add_timer(t);
}

bool Server::process_signals(bool& timeout, bool& stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret <= 0) {
        return false;
    }  else {
        for (int j = 0; j < ret; ++j) {
            switch (signals[j]) {
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
                case SIGALRM: {
                    timeout = true;
                    break;
                }
            }
        }
    }
    return true;
}

void Server::process_read(int connfd) {
    timer* t = users_timer[connfd].conn_timer;
    if (users[connfd].read()) {
        LOG_INFO("read client(%s) data", inet_ntoa(users[connfd].get_address()->sin_addr));
        m_thread_pool->append(users + connfd);
        
        // adjust timer
        if (t) {
            time_t curr = time(NULL);
            t->expire = curr + 3 * TIMESLOT;
            m_timer_list.adjust_timer(t);
            LOG_INFO("%s", "adjust timer");
        }
    } else {
        cb_func(users_timer + connfd);
        if (t) {
            m_timer_list.del_timer(t);
        }
    }
}

void Server::process_write(int connfd) {
    timer* t = users_timer[connfd].conn_timer;
    if (users[connfd].write()) {
        LOG_INFO("write client(%s) data", inet_ntoa(users[connfd].get_address()->sin_addr));
        if (t) {
            time_t curr = time(NULL);
            t->expire = curr + 3 * TIMESLOT;
            m_timer_list.adjust_timer(t);
            LOG_INFO("%s", "adjust timer");
        }
    } else {
        cb_func(users_timer + connfd);
        if (t) {
            m_timer_list.del_timer(t);
        }
    }
}

void Server::run() {
    printf("server run.\n");
    // listen socket
    // bind to any local address
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd != -1);
    if (m_opt_linger == 0) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (1 == m_opt_linger) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);
    
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    
    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    addfd(m_epollfd, m_listenfd, false);
    http_conn::m_epollfd = m_epollfd;
    
    // pipe
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false);
    
    // add signal handler
    addsig(SIGPIPE, SIG_IGN);
    addsig(SIGTERM, sig_handler);
    addsig(SIGALRM, sig_handler);
    
    // event loop
    alarm(TIMESLOT);
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            std::cout << "epoll failure.\n";
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd) {
                // new client connection
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address,
                            &client_addrlength);
                if (connfd < 0) {
                    LOG_ERROR("%s:%d", "accept error", errno);
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD) {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                init_new_conn(connfd, client_address);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // close connection
                timer* t = users_timer[sockfd].conn_timer;
                t->cb_func(&users_timer[sockfd]);
                if (t) {
                    m_timer_list.del_timer(t);
                }
            } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                // process signals
                if (!process_signals(timeout, stop_server)) {
                    LOG_ERROR("%s", "fail to process signal");
                }
            } else if (events[i].events & EPOLLIN) {
                process_read(sockfd);
            } else if (events[i].events & EPOLLOUT) {
                process_write(sockfd);
            }
        }
        
        if (timeout) {
            // LOG_INFO("%s", "Timeout, timer tick");
            m_timer_list.tick();
            alarm(TIMESLOT);
            timeout = false;
        }
    }
}