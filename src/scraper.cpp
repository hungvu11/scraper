#include "scraper.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "80" // the port client will be connecting to 

#include <chrono>
using namespace std::chrono;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
#include <chrono>
#include <string.h>
using namespace std::chrono;

Scraper::Scraper()
{
}


int Scraper::hostname_to_ip(std::string hostname , char* ip)
{
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
		
	if ( (he = gethostbyname( hostname.c_str() ) ) == NULL) 
	{
		// get the host info
		// herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}
	
	return 1;
}

void Scraper::collect_service()
{
    std::string line;
    std::ifstream myfile("sites.txt");

    if (myfile.is_open())
    {
        while (getline(myfile, line))
        {
            // std::cout << line << '\n';
            services.push_back(line);
            service_num++;
        }
        is_available.resize(service_num);
        access_time.resize(service_num);
        myfile.close();
    }

    else std::cout << "Unable to open file\n";
}

bool Scraper::check_available(std::string service, int64_t& ac_time)
{
    int sockfd, numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

	char ip[100];

	// strcpy(hostname, service.c_str()); 
	hostname_to_ip(service, ip);
    ac_time = 0.0;

    if ((rv = getaddrinfo(ip, "80", &hints, &servinfo)) != 0) {
        // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }

    auto start = high_resolution_clock::now();
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return false;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    // printf("client: connecting to %s\n", s);

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    
    ac_time = duration.count();

    freeaddrinfo(servinfo); // all done with this structure

    close(sockfd);

    return true;
}

void Scraper::collect_availability()
{
    sorted_access_time.clear();
    for (int i=0; i<service_num; i++)
    {
        is_available[i] = check_available(services[i], access_time[i]);
        if (access_time[i] != 0)
            sorted_access_time.insert({access_time[i], i});
        // std::cout << access_time[i] << " ms" << std::endl;
    }
}

std::string Scraper::find_service_with_mininum_at()
{
    auto p = *sorted_access_time.begin();
    return services[p.second];
}

std::string Scraper::find_service_with_maximum_at()
{
    auto p = sorted_access_time.end();
    --p;
    return services[(*p).second];
}
