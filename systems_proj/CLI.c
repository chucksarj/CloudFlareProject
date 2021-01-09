/* Basic CLI tool in C modeled off of a loadbalancer and httpserver I wrote */
/* url parser - influence from stack overflow */
/* Environment written/tested on: Ubuntu 18.04.4 LTS */
/* program is not really built to gracefully handle failures, but I used EPOLL in
 * an effort to at least know when errors happen better */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_SIZE 4096

static int debug = 0;
struct sockaddr_in serv_addr;

// our profile struct
typedef struct {
    int                 NR;                     // The number of requests
    double              f_time;                 // The fastest time
    double              s_time;                 // The slowest time
    float               mean_t;                 // The mean time
    double              med_t;                  // The median time
    float               p;                      // The percentage of requests that succeeded
    int                 *codes;                 // All codes returned
    int                 s_bytes;                // The size in bytes of the smallest response
    int                 l_bytes;                // The size in bytes of the largest response
} req_Stats;

// our url parser struct
typedef struct {
    const char*         protocol;
    const char*         host;
    const char*         port;
    unsigned short      sport;
    const char*         path;
} URL_INFO;

// URL parser
URL_INFO* split_url(URL_INFO* info, const char* url) {
    if (!info || !url) {
        return NULL;
    }

    info->protocol = strtok(strcpy((char*)malloc(strlen(url)+1), url), "://");
    info->host = strstr(url, "://");

    if (info->host) {
        info->host += 3;
        char* host_port_path = strcpy((char*)calloc(1, strlen(info->host) + 1), info->host);
        info->host = strtok(host_port_path, ":");
        info->host = strtok(host_port_path, "/");
    } else {
        char* host_port_path = strcpy((char*)calloc(1, strlen(url) + 1), url);
        info->host = strtok(host_port_path, ":");
        info->host = strtok(host_port_path, "/");
    }

    char* URL = strcpy((char*)malloc(strlen(url) + 1), url);
    info->port = strstr(URL + 6, ":");
    char* port_path = 0;
    char* port_path_copy = 0;

    if (info->port && isdigit(*(port_path = (char*)info->port + 1))) {
        port_path_copy = strcpy((char*)malloc(strlen(port_path) + 1), port_path);
        char * r = strtok(port_path, "/");
        if (r)
            info->port = r;
        else
            info->port = port_path;
    } else {
        info->port = "80";
    }

    if (port_path_copy) {
        info->path = port_path_copy + strlen(info->port ? info->port : "");
    } else {
        char* path = strstr(URL + 8, "/");
        info->path = path ? path : "/";
    }

    int r = strcmp(info->protocol, info->host) == 0;
    if (r && (strcmp(info->port, "80") == 0)) {
        info->protocol = "http";
    } else if (r) {
        info->protocol = "tcp";
    }

    return info;
}

// make our socket non blocking
static int make_socket_non_blocking(int sfd) {
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        printf("fcntl\n");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        printf("fcntl\n");
        return -1;
    }

    return 0;
}

// utility function: find the max of an array
double find_max(int n, double arr[]) {
    int i;
    double max = arr[0];

    for (i = 1; i < n; i++)
        if (arr[i] > max)
            max = arr[i];

    return max;
}

// utility function: find the min of an array
double find_min(int n, double a[]) {
    int c; 
    double min = a[0];

    for (c = 1; c < n; c++)
        if (a[c] < min)
            min = a[c];

    return min;
}

// utility function: find the mean of an array 
float mean(int m, double a[]) {
    double sum = 0;
    int i;

    for (i = 0; i < m; i++)
        sum += a[i];

    return (float)sum / m;
}

// utility function: find the median of an array
double median(int n, double x[]) {
    double temp;
    int i, j;
    
    for (i = 0; i < n-1; i++) {
        for (j = i+1; j < n; j++) {
            if (x[j] < x[i]) {
                // swap elements
                temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }

    if (n%2 == 0) {
        return ((double)((x[n / 2] + x[n/2 - 1]) / 2.0));
    } else {
        return ((double)x[n / 2]);
    }
}

// display stats utility function when in profile mode
void display_stats(req_Stats* r) {
    printf("Number of requests: %d\n", r->NR);
    printf("Fastest Time: %f seconds\n", r->f_time);
    printf("Slowest Time: %f seconds\n", r->s_time);
    printf("Mean Time: %f seconds\n", r->mean_t);
    printf("Median Time: %f seconds\n", r->med_t);
    printf("Percent of Successful Requests: %.2f%%\n", r->p);
    printf("Error Codes (if any): ");
    
    for (int i = 0; i < r->NR; i++) {
        if (r->codes[i] != 200 && r->codes[i] != 0) {
            printf("%d ", r->codes[i]);
        }
    }
    
    printf("\n");
    printf("Smallest Response: %d bytes\n", r->s_bytes);
    printf("Largest Response: %d bytes\n", r->l_bytes);
}

// set up our hostnet before opening our socket connection
void SocketSet_Up(URL_INFO* info) {
    struct hostent *server;
    
    /* lookup the ip address */
    server = gethostbyname(info->host);
    if (server == NULL) {
        printf("invalid host\n");
    }

    info->sport = atoi(info->port);

    /* fill in the structure */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(info->sport);
    memcpy(&serv_addr.sin_addr, server->h_addr_list[0], server->h_length);

    if (debug) {
        printf("Before connect %s:%d len=%d\n", 
                inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port), server->h_length);
    }
}

// the handle responses from socket based off of what Epoll wait tells us
int epoll_activity(struct epoll_event *evs, req_Stats *r, int sockfd, int flag, int x, int i) {
    char buffer[MAX_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int size_recv = 0; 

    // epoll error, otherwise we read
    if ((evs[0].events & EPOLLERR) ||
            (evs[0].events & EPOLLHUP)) {
        
        printf("epoll error\n");

        return -1;

    } else if (evs[0].events & EPOLLIN) {
        if ((size_recv = recv(sockfd, buffer, MAX_SIZE, 0)) < 0) {
            printf("recv error: %d\n", size_recv);

            return -1;
        }

        if (size_recv == 0) {
            return -1;
        }

        if (flag) {
            if (x) {
                if (debug) {
                    printf("%.*s\n", 3, &buffer[9]);
                    printf("recieved: %d\n", size_recv);
                }

                char tmp[3];
                strncpy(tmp, &buffer[9], 3);

                r->codes[i] = atoi(tmp);
            }
        } else {
            printf("%s" , buffer);
        }

    }

    return size_recv;
}

// The engine of the program, handle setting up socket, sending, and if needed
// displaying stats in profile mode
void SocketSR(URL_INFO* info, int NR, int flag) {
    int epollfd, n, s, n_Epoll, bytes, successes = 0, sent = 0, s_bytes = 0, l_bytes = 0;
    struct epoll_event ev;
    char request[512];
    double times[NR];
    req_Stats r;
    struct timeval start, end; 

    // set up our profile
    if (flag) {
        memset(&r, 0, sizeof(r));
        r.NR = NR;
        r.codes = (int *) calloc(NR, sizeof(int));
    }

    // format request string - type unspecified so going with GET
    n = sprintf(request, "GET %s HTTP/1.0\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\nHost: %s:%s\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate\r\n\r\n", info->path, info->host, info->port);

    if (debug) {
        printf("sprintf wrote: %d bytes\n", n); 
    }

    // create our epoll file descriptor 
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        printf("epoll_create\n");
        abort();
    }

    for (int i = 0; i < NR; i++) {
        int total_size = 0;
        int x = 1;
        struct epoll_event evs;

        int sockfd =  open("/dev/null", O_RDONLY);
        
        if (debug) {
            printf("\ni: %d, x: %d\n", i, x);
        }
       
        if (flag) {
            gettimeofday(&start, NULL);
        }
    
        /* create the socket */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            printf("failed to create socket\n");
        }
        
        if (debug) {
            printf("FD: %d, Before connect %s:%d\n", 
                    sockfd, inet_ntoa(serv_addr.sin_addr), 
                    ntohs(serv_addr.sin_port));
        }

        /* connect the socket */
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("unable to connect to server\n");
        }
        
        // don't really know if this is needed, but decided it was better
        // to use EPOLL without edge triggered
        if (make_socket_non_blocking(sockfd) < 0) {
            printf("unable to make socket non blocking\n");
        }
        
        // sending message to our socket
        // *took me forever to find bug in code that I forgot to reset 
        // sent var*
        sent = 0;
        while (sent < n) {
            bytes = send(sockfd, request + sent, n - sent, 0);

            if (bytes < 0)
                perror("ERROR writing message to socket");
            if (bytes == 0)
                break;

            sent += bytes;
        }

        ev.events = EPOLLIN;

        s = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
        if (s == -1) {
            printf("epoll_ctl\n");
            abort ();
        }

        memset(&evs, 0, sizeof(evs));

        while(1) {
            n_Epoll = epoll_wait(epollfd, &evs, 1, 100); // 100 ms is plenty of time
            
            if (debug) {
                printf("n_Epoll=%d errno=%d\n", n_Epoll, errno);
            }

            if (n_Epoll > 0) {
                int ret = epoll_activity(&evs, &r, sockfd, flag, x, i);
                
                if (ret < 0) {
                    break;
                }
                
                total_size += ret;

                x = 0;

            } else if (n_Epoll == 0) {
                // epoll timed out 

                if (debug) {
                    if (evs.events & EAGAIN) {
                        printf("eagain\n");
                    }
                }

                continue;
            } else {
                // epoll threw an error
                printf("epoll threw an error %d\n", errno);
                break;
            }

        }

        if (flag) {
            // end timer
            gettimeofday(&end, NULL);

            // so things dont look so barren to user
            printf("Request %d: sent: %d bytes recieved: %d bytes Response Code: %d \n", 
                    i + 1, sent, total_size, r.codes[i]);

            double time_taken = (end.tv_sec - start.tv_sec) * 1e6;
            time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
            
            times[i] = time_taken;

            // check for smallest and largest bytes
            if (i == 0) {
                s_bytes = total_size;
            } else if (s_bytes > total_size) {
                s_bytes = total_size;
            }
            if (l_bytes < total_size) {
                l_bytes = total_size;
            }
        }
        
        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &ev);

        /* close the socket */
        shutdown(sockfd, 2);
        close(sockfd);
    }
   
    if (flag) {
        printf("\n");
    }

    // display our stats in profile mode
    if (flag) {
        for (int l = 0; l < NR; l++) {
            if (r.codes[l] == 200) {
                successes++;
            }
        }

        float percentage = (float)successes / NR * 100.0;
        r.p = percentage;
    
        r.f_time = find_min(NR, times);
        r.s_time = find_max(NR, times);
        r.mean_t = mean(NR, times);
        r.med_t = median(NR, times);
        r.s_bytes = s_bytes;
        r.l_bytes = l_bytes;

        display_stats(&r);
    }

    free(r.codes);
}

// usage util function for getopt
void usage(char *argv, char *msg) {
    printf("%s: %s\n", argv, msg);
    printf("\t--debug or -d debug\n");
    printf("\t--help or -h help\n");
    printf("\t--url or -u specify url to request *required*\n");
    printf("\t--profile or -p specify number of requests & show profile\n");
    exit(0);
}

// driver code
int main(int argc, char **argv) {
    int c;
    int num_Req = 1;    // 1 by default
    int flag = 0;       // are we in profile mode
    char *url = NULL;   // url string
    URL_INFO info;      // url parser structure
    
    // getopt parse args
    static struct option long_options[] = {
        {"debug",   no_argument,       0, 'd'},
        {"help",    no_argument,       0, 'h'},
        {"url",     required_argument, 0, 'u'},
        {"profile", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    opterr = 0;
    while (optind < argc) {
        int option_index = 0;

        if ((c = getopt_long(argc, argv, "dhu:p:", long_options, &option_index)) != -1) {
            if (debug) {
                printf("argc = %d optind = %d c = %c\n", argc, optind, c);
            }

            switch (c) {
                case 'd':
                    debug = 1;
                    break;
                case 'h':
                    usage(argv[0], "");
                    break;
                case 'u':
                    url = optarg;
                    break;
                case 'p':
                    if (optarg && optarg[0] != '-') {
                        num_Req = atoi(optarg);
                        flag = 1;

                        if (num_Req < 0) {
                            usage(argv[0], "invalid args passed for --profile");
                        }
                    } else {
                        usage(argv[0], "invalid args passed for --profile");
                    }
                    break;
                case '?':
                    if (optopt == 'u') {
                        usage(argv[0], "Needs a url");
                    } else if (optopt == 'p') {
                        usage(argv[0], "Needs a valid positive integer");
                    } else {
                        usage(argv[0], "Invalid Option");
                    }

                    break;
                default:
                    usage(argv[0], "Invalid Option");
                    break;
            }
        } else {
            usage(argv[0], "Needs option");
            break;
        }
    }

    // we need a url to run program
    if (url == NULL) {
        usage(argv[0], "URL not specified");
    }
    if (debug) {
        printf("debug %d, url %s, num requests %d\n", debug, url, num_Req);
    }

    // parse our url
    split_url(&info, url);
    if (debug) {
        printf("Protocol: %s\nSite: %s\nPort: %s\nPath: %s\n", info.protocol, info.host, info.port, info.path);
    }

    // supporting HTTPS would require client server certificate exchange and encryption 
    if (strcasecmp(info.protocol, "HTTPS") == 0) {
        printf("URL: %s\n-> CLI does not support HTTPS!\n", url);
        exit(0);
    }

    // set out hostnet
    SocketSet_Up(&info);

    // socket connection + S/R with server
    SocketSR(&info, num_Req, flag);
    
    return EXIT_SUCCESS;
}

