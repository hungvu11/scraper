#include "server.h"

server::server()
{
    my_scraper = new Scraper();
}

void server::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl()");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl()");
    }
}

void server::control_epoll_event(int epoll_fd, int op, int fd, uint32_t events, void* data)
{
    if (op == EPOLL_CTL_DEL) {
        if (epoll_ctl(epoll_fd, op, fd, NULL) < 0) {
            perror("del event");
            exit(EXIT_FAILURE);
        }
    }
    else {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        ev.data.ptr = data;
        if (epoll_ctl(epoll_fd, op, fd, &ev) < 0) {
            perror("add event");
            exit(EXIT_FAILURE);
        }
    }
}

void server::handleConnection(epoll_info* epoll_in)
{
    // epoll_info* epoll_in = (epoll_info*)args;
    int fd = epoll_in->epoll_fd;
    bool active = true;
    for (;;) {
        if (!active) usleep(100);
        int nevents = epoll_wait(fd, epoll_in->events, MAXEVENTS, -1);
        if (nevents == -1) {
            perror("epoll_wait()");
            exit(EXIT_FAILURE);
        }
        else if (nevents == 0) {
            active = false;
            continue;
        }
        active = true;
        for (int i = 0; i < nevents; i++) {

            struct epoll_event ev = epoll_in->events[i];
            if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
                // error case
                perror("epoll error");
                control_epoll_event(fd, EPOLL_CTL_DEL, ev.data.fd, 0);
                close(ev.data.fd);
            }
            else if (ev.events & EPOLLIN) {
                // read from client
                char buf[4096];
                ssize_t nbytes = recv(ev.data.fd, buf, sizeof(buf), 0);
                if (nbytes == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { // retry
                        control_epoll_event(fd, EPOLL_CTL_MOD, ev.data.fd, EPOLLIN | EPOLLET);
                    }
                    else { // other error
                        perror("read()");
                        control_epoll_event(fd, EPOLL_CTL_DEL, ev.data.fd, 0);
                        close(ev.data.fd);
                    }
                }
                else if (nbytes == 0) { // close connection
                    control_epoll_event(fd, EPOLL_CTL_DEL, ev.data.fd, 0);
                    close(ev.data.fd);
                }
                else { // fully receive the message
                    event_data* ed;
                    ed->fd = ev.data.fd;
                    memcpy(ed->buffer, buf, 4096); 

                    control_epoll_event(fd, EPOLL_CTL_MOD, ev.data.fd, EPOLLOUT | EPOLLET, ed);
                    // fwrite(buf, sizeof(char), nbytes, stdout);
                }
            }
            else if (ev.events & EPOLLOUT) {
                // write a response to client
                std::string response;
                char* buf = (char *) ev.data.ptr;
                std::cout << buf << std::endl;
                char* request1 = (char *) ev.data.ptr;
                const char* request = "1";
                if (strcmp(request, "1") == 0) {
                    response = "Service with minimum access time is: ";
                    response += my_scraper->find_service_with_mininum_at();
                }
                else if (strcmp(request, "2") == 0) {
                    response = "Service with maximum access time is: ";
                    response = my_scraper->find_service_with_maximum_at();
                } else {
                    response = "Invalid type of service";
                }
                response += '\n';
                ssize_t nbytes = send(ev.data.fd, response.c_str(), response.length(), 0);
                if (nbytes == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { // retry
                        control_epoll_event(fd, EPOLL_CTL_MOD, ev.data.fd, EPOLLOUT | EPOLLET);
                    }
                    else {
                        perror("read()");
                        control_epoll_event(fd, EPOLL_CTL_DEL, ev.data.fd, 0);
                        close(ev.data.fd);
                    }
                }
                else {

                    control_epoll_event(fd, EPOLL_CTL_MOD, ev.data.fd, EPOLLIN | EPOLLET);
                }

            }
            else { // somthing unexpected
                control_epoll_event(fd, EPOLL_CTL_DEL, ev.data.fd, 0);
                close(ev.data.fd);
            }
        }
    }
}

int server::create_server() {
    // create the server socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket()");
        return 1;
    }
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        perror("setsockopt()");
        return 1;
    }

    // bind
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind()");
        return 1;
    }

    // make it nonblocking, and then listen
    set_nonblocking(sock);
    if (listen(sock, SOMAXCONN) < 0) {
        perror("listen()");
        return 1;
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        // create the epoll socket
        epolls[i].epoll_fd = epoll_create1(0);
        if (epolls[i].epoll_fd == -1) {
            perror("epoll_create1()");
            return 1;
        }
        epolls[i].events = (epoll_event*) malloc(MAXEVENTS * sizeof(struct epoll_event));
    }

    std::thread th[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; i++) {
        th[i] = std::thread(&server::handleConnection, this, &epolls[i]);
    }

    std::thread thread_sc(&server::collect_data, this);

    bool active = true;
    int current_index = 0;
    while (1)
    {
        // accept new conections
        if (!active) usleep(100);
        int new_socket;
        if ((new_socket = accept(sock, (struct sockaddr*)&addr, (socklen_t*)&addrlen)) == -1)
        {
            active = false;
            continue;
        }
        active = true;
        // distribute the connections to different threads
        set_nonblocking(new_socket);
        control_epoll_event(epolls[current_index].epoll_fd, EPOLL_CTL_ADD, new_socket, EPOLLIN | EPOLLET);

        current_index++;
        if (current_index == THREAD_NUM) current_index = 0;
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        th[i].join();
    }
    thread_sc.join();

    for (int i = 0; i < THREAD_NUM; i++) {
        close(epolls[i].epoll_fd);
    }
    close(sock);
    return 0;
}

void server::collect_data()
{
    my_scraper->collect_service();
    while (1)
    {
        my_scraper->collect_availability();
        sleep(2);
    }
    
}
