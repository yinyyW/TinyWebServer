#include "../include/sql_connection_pool.h"
#include <mysql/mysql.h>

connection_pool::connection_pool() {
    m_currConn = 0;
    m_freeConn = 0;
}

connection_pool::~connection_pool() {
    destroyPool();
}

connection_pool *connection_pool::getInstance() {
    static connection_pool connPool;
	return &connPool;
}

void connection_pool::connection_pool::init(std::string url, std::string user, std::string passWord, 
                                            std::string databaseName, int port, int maxConn, int close_log) {
                                            
    m_url = url;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_port = port;
    m_close_log = close_log;
    
    for (int i = 0; i < maxConn; ++i) {
        MYSQL* con = NULL;
        con = mysql_init(con);
        if (con == NULL) {
			std::cout << "Error:" << mysql_error(con);
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), user.c_str(), 
		                        passWord.c_str(), databaseName.c_str(), 
		                        port, NULL, 0);

		if (con == NULL) {
			std::cout << "Error: " << mysql_error(con);
			exit(1);
		}
		connList.push_back(con);
		++m_freeConn;
    }
    m_maxConn = m_freeConn;
    reserve = sem(m_maxConn);
}

MYSQL* connection_pool::getConnection() {
    if (connList.size() == 0) {
        return NULL;
    }
    
    MYSQL* con = NULL;
    reserve.wait();
    lock.lock();
    
    con = connList.front();
	connList.pop_front();
	
	--m_freeConn;
	++m_currConn;

	lock.unlock();
	return con;
}

bool connection_pool::releaseConnection(MYSQL *conn) {
    if (conn == NULL) {
        return false;
    }
    
    lock.lock();

	connList.push_back(conn);
	++m_freeConn;
	--m_currConn;

	lock.unlock();

	reserve.post();
	return true;
}

int connection_pool::getFreeConn() {
    return m_freeConn;
}

void connection_pool::destroyPool() {
    lock.lock();
    if (connList.size() > 0) {
        for (auto it = connList.cbegin(); it != connList.cend(); ++it) {
            mysql_close(*it);
        }
        m_currConn = 0;
		m_freeConn = 0;
		connList.clear();
    }
    lock.unlock();
}

connectionRAII::connectionRAII(MYSQL** conn, connection_pool* pool) {
    *conn = pool->getConnection();
    poolRAII = pool;
    connRAII = *conn;
}

connectionRAII::~connectionRAII() {
    poolRAII->releaseConnection(connRAII);
}