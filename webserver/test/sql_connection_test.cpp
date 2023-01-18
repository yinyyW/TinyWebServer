#include <mysql/mysql.h>
#include <string>
#include "../include/sql_connection_pool.h"

int main() {
    // init connection pool
    connection_pool *conn_pool = connection_pool::getInstance();
    conn_pool->init("localhost", "root", "comp9313", 
                    "tiny_web_server", 3306, 5, true);
    
    // init connectionRAII
    MYSQL* sql = NULL;
    connectionRAII sql_conn = connectionRAII(&sql, conn_pool);
    
    // query
    if (mysql_query(sql, "SELECT username, password FROM user")) {
        std::cout << "select error: " << mysql_error(sql) << std::endl;
        return 1;
    }
    MYSQL_RES *result = mysql_store_result(sql);
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        std::cout << "username: " << std::string(row[0]) << std::endl;
        std::cout << "password: " << std::string(row[1]) << std::endl;
    }
    return 0;
}