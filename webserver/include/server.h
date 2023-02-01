#ifndef SERVER_H
#define SERVER_H

#include "http_conn.h"
#include "threadpool.h"
#include "log.h"
#include "timer.h"


const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

class Server {
    public:
        Server();
        ~Server();
        void init(int port, int close_log, int async_log, int opt_linger,
                int thread_num, int db_max_conn, std::string database_name,
                std::string database_username, std::string database_password);
        void run();
    
    private:
        // HELPER FUNCTIONS
        void init_log();
        void init_thread_pool();
        void init_conn_pool();
        void init_new_conn(int connfd, struct sockaddr_in client_address);
        bool process_signals(bool& timeout, bool& stop_server);
        void process_read(int connfd);
        void process_write(int connfd);
        
        // UTIL FUNCTIONS
        int setnonblocking(int fd);
        void addfd(int epollfd, int fd, bool one_shot);
        static void sig_handler(int sig);
        void addsig(int sig, void(handler)(int), bool restart = true);
        void show_error(int connfd, const char* info);
        static void cb_func(client_data* user_data);

    public:
        // thread pool
        threadpool<http_conn>* m_thread_pool;
        int m_thread_num;
        
        // database info
        std::string m_db_name;
        std::string m_db_user;
        std::string m_db_password;
        int m_db_max_conn;
        connection_pool* m_conn_pool;
        
        // log
        int m_close_log;
        int m_async_log;
        
        // server info
        char* m_doc_root;
        int m_listenfd;
        int m_port;
        int m_opt_linger;
        static int m_epollfd;
        static int m_pipefd[2];
        
        // users
        http_conn* users;
        client_data* users_timer;
        timer_lst m_timer_list;
};

#endif