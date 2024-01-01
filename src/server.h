#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <thread>
#include "scraper.h"

#define MAXEVENTS 10000
#define THREAD_NUM 5
#define PORT 8080

typedef struct epoll_info
{
    int epoll_fd;
    struct epoll_event* events;
} epoll_info;

typedef struct event_data
{
    int fd;
    char buffer[4096];
} event_data;

class server
{
public:
    void set_nonblocking(int fd);
    void control_epoll_event(int epoll_fd, int op, int fd, uint32_t events, void *data = nullptr);
    void handleConnection(epoll_info* epoll_in);
    int create_server();
    void collect_data();
    server();

private:
    int sock;
    epoll_info epolls[THREAD_NUM];
    Scraper* my_scraper;
};
