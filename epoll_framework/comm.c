#include "globals.h"

double current_dtime;
time_t sys_curtime;
struct timeval current_time;


int Biggest_FD = 1024;  /*默认的最大文件描述符数量 1024*/
static int MAX_POLL_TIME = 1000;	/* see also comm_quick_poll_required() */
fde *fd_table = NULL;	

time_t getCurrentTime(void)
{
    gettimeofday(&current_time, NULL);
    current_dtime = (double) current_time.tv_sec +
	(double) current_time.tv_usec / 1000000.0;
    return sys_curtime = current_time.tv_sec;
}


void
comm_close(int fd)
{
	assert(fd>0);
	fde *F = &fd_table[fd];
	if(F) memset((void *)F,'\0',sizeof(fde));
	epollSetEvents(fd, 0, 0);
	close(fd);
}

void
comm_init(int max_fd)
{
	if(max_fd > 0 ) Biggest_FD = max_fd;
	fd_table = calloc(Biggest_FD, sizeof(fde));
    do_epoll_init(Biggest_FD);
}



void
comm_select_shutdown(void)
{
    do_epoll_shutdown();
	if(fd_table) free(fd_table);
}


//static int comm_select_handled;


inline void
comm_call_handlers(int fd, int read_event, int write_event)
{
    fde *F = &fd_table[fd];
    
    debug(5, 8) ("comm_call_handlers(): got fd=%d read_event=%x write_event=%x F->read_handler=%p F->write_handler=%p\n"
	,fd, read_event, write_event, F->read_handler, F->write_handler);
    if (F->read_handler && read_event) {
	    PF *hdl = F->read_handler;
	    void *hdl_data = F->read_data;
	    /* If the descriptor is meant to be deferred, don't handle */

		debug(5, 8) ("comm_call_handlers(): Calling read handler on fd=%d\n", fd);
		//commUpdateReadHandler(fd, NULL, NULL);
		hdl(fd, hdl_data);
    }
	
    if (F->write_handler && write_event) {
	
	    PF *hdl = F->write_handler;
	    void *hdl_data = F->write_data;
	
	    //commUpdateWriteHandler(fd, NULL, NULL);
	    hdl(fd, hdl_data);
    }
}


int
commSetTimeout(int fd, int timeout, PF * handler, void *data)
{
    fde *F;
    debug(5, 3) ("commSetTimeout: FD %d timeout %d\n", fd, timeout);
    assert(fd >= 0);
    assert(fd < Biggest_FD);
    F = &fd_table[fd];

	
    if (timeout < 0) {
	F->timeout_handler = NULL;
	F->timeout_data = NULL;
	return F->timeout = 0;
    }
    assert(handler || F->timeout_handler);
    if (handler || data) {
	F->timeout_handler = handler;
	F->timeout_data = data;
    }
    return F->timeout = sys_curtime + (time_t) timeout;
}

void
commUpdateReadHandler(int fd, PF * handler, void *data)
{
    fd_table[fd].read_handler = handler;
    fd_table[fd].read_data = data;
    
    epollSetEvents(fd,1,0); 
}

void
commUpdateWriteHandler(int fd, PF * handler, void *data)
{
    fd_table[fd].write_handler = handler;
    fd_table[fd].write_data = data;
	
    epollSetEvents(fd,0,1); 
}



static void
checkTimeouts(void)
{
    int fd;
    fde *F = NULL;
    PF *callback;

    for (fd = 0; fd <= Biggest_FD; fd++) {
	F = &fd_table[fd];
	/*if (!F->flags.open)
	    continue;
	*/
	
	if (F->timeout == 0)
	    continue;
	if (F->timeout > sys_curtime)
	    continue;
	debug(5, 5) ("checkTimeouts: FD %d Expired\n", fd);
	
	if (F->timeout_handler) {
	    debug(5, 5) ("checkTimeouts: FD %d: Call timeout handler\n", fd);
	    callback = F->timeout_handler;
	    F->timeout_handler = NULL;
	    callback(fd, F->timeout_data);
	} else {
	    debug(5, 5) ("checkTimeouts: FD %d: Forcing comm_close()\n", fd);
	    comm_close(fd);
	}
    }
}


int
comm_select(int msec)
{
    static double last_timeout = 0.0;
    int rc;
    double start = current_dtime;

    debug(5, 3) ("comm_select: timeout %d\n", msec);

    if (msec > MAX_POLL_TIME)
	msec = MAX_POLL_TIME;


    //statCounter.select_loops++;
    /* Check timeouts once per second */
    if (last_timeout + 0.999 < current_dtime) {
	last_timeout = current_dtime;
	checkTimeouts();
    } else {
	int max_timeout = (last_timeout + 1.0 - current_dtime) * 1000;
	if (max_timeout < msec)
	    msec = max_timeout;
    }
    //comm_select_handled = 0;

    rc = do_epoll_select(msec);


    getCurrentTime();
    //statCounter.select_time += (current_dtime - start);

    if (rc == COMM_TIMEOUT)
	debug(5, 8) ("comm_select: time out\n");

    return rc;
}


const char *
xstrerror(void)
{
    static char xstrerror_buf[BUFSIZ];
    const char *errmsg;

    errmsg = strerror(errno);

    if (!errmsg || !*errmsg)
	errmsg = "Unknown error";

    snprintf(xstrerror_buf, BUFSIZ, "(%d) %s", errno, errmsg);
    return xstrerror_buf;
}


int
ignoreErrno(int ierrno)
{
    switch (ierrno) {
    case EINPROGRESS:
    case EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK
    case EAGAIN:
#endif
    case EALREADY:
    case EINTR:
#ifdef ERESTART
    case ERESTART:
#endif
	return 1;
    default:
	return 0;
    }
    /* NOTREACHED */
}


