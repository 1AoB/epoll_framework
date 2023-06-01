srcs = main.c comm_epoll.c comm.c
cflags = -g -w
objects        := $(srcs:.c=.o)

epoll_demo : $(objects)                                                   
	gcc -o  epoll_demo $(objects)                                        

.c.o:       
	gcc $(cflags)  -c    $<

.PHONY : clean                                            
clean :                                                                  
	-rm epoll_demo  $(objects)                                         
