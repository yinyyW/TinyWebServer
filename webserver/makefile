CXX ?= g++

server: main.cpp  src/http_conn.cpp src/log.cpp src/server.cpp src/sql_connection.cpp src/timer.cpp
	$(CXX) -o server  $^ -lmysqlclient

clean:
	rm  -r server