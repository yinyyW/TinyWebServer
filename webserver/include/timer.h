#ifndef TIMER_H
#define TIMER_H

#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

class timer;

struct client_data {
    sockaddr_in client_addr;
    int sockfd;
    timer* conn_timer;
};

class timer {
    public:
        timer() : prev(NULL), next(NULL) {}
    public:
        timer* prev;
        timer* next;
        time_t expire;
        void (* cb_func)(client_data *);
        client_data *user_data;
};

class timer_lst {
    public:
        timer_lst();
        ~timer_lst();
        
        void add_timer(timer* t);
        void adjust_timer(timer* t);
        void del_timer(timer* t);
        void tick();
    
    private:
        timer* head;
        timer* tail;
        void (*cb_func)(client_data*);
};

#endif