gobang:gobang.cc
	g++ $^ -o $@ -std=c++11 -L/lib64/mysql -lmysqlclient -ljsoncpp -lboost_system
.PHONY:clean
clean:
	rm -rf gobang