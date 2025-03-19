gobang:gobang.cc
	g++ $^ -o $@ -std=c++11 -L/lib64/mysql -lmysqlclient -ljsoncpp -lboost_system -lpthread -g
.PHONY:clean
clean:
	rm -rf gobang