#include "server.h"
#include "scraper.h"
#include <thread>
Scraper* scraper;



int main()
{
   
    server my_server;
    my_server.create_server();

    return 0;

}