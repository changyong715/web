cc=gcc
src=epoll_server.c
bin=epoll_server

$(bin):$(src)
	$(cc) -o $@ $^ 

.PHONY:clean
clean:
	rm -f $(bin)
