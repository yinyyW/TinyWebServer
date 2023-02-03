#include "include/server.h"
#include <bits/getopt_core.h>
#include <cstdlib>
#include <stdlib.h>

void parseArgs(int argc, char* argv[], int& port, int& log_write, int& opt_linger,
                int& sql_conn_num, int& thread_num, int& close_log) {
    int opt;
    const char *str = "p:l:o:s:t:c:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 'p': {
                port = atoi(optarg);
                break;
            }
            case 'l': {
                log_write = atoi(optarg);
                break;
            }
            case 'o': {
                opt_linger = atoi(optarg);
                break;
            }
            case 's': {
                sql_conn_num = atoi(optarg);
                break;
            }
            case 't': {
                thread_num = atoi(optarg);
                break;
            }
            case 'c': {
                close_log = atoi(optarg);
                break;
            }
        }
    }
}

int main(int argc, char* argv[], char *envp[]) {
    // database info
    // char* db_name = getenv("TINY_WEBSERVER_DBNAME");
    // char* db_user = getenv("TINY_WEBSERVER_DBUSER");
    // char* db_password = getenv("TINY_WEBSERVER_DBPASSWORD");
    // printf("db name: %s, db user: %s, db password: %s.\n", db_name, db_user, db_password);
    
    // parse arguments
    int port = 8899;
    int log_write = 1;
    int opt_linger = 0;
    int sql_conn_num = 8;
    int thread_num = 8;
    int close_log = 0;
    parseArgs(argc, argv, port, log_write, opt_linger, sql_conn_num, thread_num, close_log);
    
    // printf("port: %d, log_write: %d, opt_linger: %d, sql_conn: %d, thread_num: %d, close_log: %d.\n",
    //         port, log_write, opt_linger, sql_conn_num, thread_num, close_log);
    // start server
    Server server;
    server.init(port, close_log, log_write, opt_linger, thread_num,
                sql_conn_num, "tiny_web_server", 
                "root", "comp9313");
    server.run();
    return 0;
}