/*
 * CS3214 Project 5 (sysstatd)
 * System Status Web Service
 * Jaeil Yi
 */

#include <error.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/poll.h>
#include "csapp.h"
#include "server.h"
#include "json.h"
#include "rio.h"

#define BACKLOG 20

/*global variables*/
int memory_index = 0;
char *memories[1024];
pthread_mutex_t lock;

//arguments for thread routine
struct thread_args {
    int fd;
    struct sockaddr_storage sockstor;
    socklen_t sockstor_len;
    char* root;
};

/*
 * Main loop.
 * */
int main(int argc, char **argv)
{
    int opt;
    char *port, *root = NULL;
    char *relay = NULL;

    //parse commandline arguments
    while ((opt = getopt(argc, argv, "p:r:R:")) > 0)
    {
        switch(opt)
        {
            case 'p':
                port = optarg;
                break;
            case 'r':  // -r relayhost:port
                relay = optarg;
                break;
            case 'R':  // -R path
                root = optarg;
                break;
        }
    }

    //initialize lock
    if (pthread_mutex_init(&lock, NULL) != 0)
        fprintf(stderr, "mutex_init error\n");

    struct addrinfo *result;
    //specify options for addrinfo
    struct addrinfo hints;
    memset(&hints, '\0', sizeof(hints));


    //relay mode
    if (relay != NULL)
    {
        hints.ai_flags = AI_ADDRCONFIG;
        hints.ai_socktype = SOCK_STREAM;

        char *next, *ptr;
        char *relay_server, *relay_port;

        ptr = relay;
        next = strtok_r(ptr, ":", &relay_port);
        relay_server = next;

        printf("running relay mode...\n");
        while (1) {
        int r;
        r = getaddrinfo(relay_server, relay_port, &hints, &result);

        if (r != 0)
            error (EXIT_FAILURE, 0, "getaddrinfo: %s\n", gai_strerror(r));

        struct addrinfo *runp = result;

        while (runp != NULL)
        {
            int skt = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);

            if (skt != -1)
            {
                int cnt;
                if ((cnt = connect(skt, runp->ai_addr, runp->ai_addrlen)) == 0)
                {
                    writen(skt, "group398\r\n", strlen("group398\r\n"));

                    handle_request(skt, root);
                }
                else
                    error (0, 0, "cannot contact %s\n", relay_server);

                close(skt);
            }

            runp = runp->ai_next;
        }

        freeaddrinfo(result);
        } //end infinite while loop


    }
    else //non relay mode
    {
        hints.ai_family = AF_INET6; //allow both IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; //TCP stream socket
        hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; //fill in my IP
        hints.ai_protocol = 0; //accept any protocol
		int r;
		r = getaddrinfo(NULL, port, &hints, &result);


		if (r != 0)
		{
		    error (EXIT_FAILURE, 0, "getaddrinfo: %s", gai_strerror(r));
		}

		int nfds = 0;
		struct addrinfo *runp = result;
		while (runp != NULL)
		{
		    ++nfds;
		    runp = runp->ai_next;
		}

		struct pollfd fds[nfds];

		/* go through the list of address structures to try to
		 * bind.
		 */
		for(nfds = 0, runp = result; runp != NULL; runp = runp->ai_next)
		{
		    fds[nfds].fd = socket(runp->ai_family,
		    				runp->ai_socktype, runp->ai_protocol);
		    if (fds[nfds].fd == -1) //socket failed
		    {
		        error (EXIT_FAILURE, errno, "socket");
		    }
		    fds[nfds].events = POLLIN;

		    //set socket options
		    int opt = 1;
		    setsockopt(fds[nfds].fd, SOL_SOCKET,
		    			SO_REUSEADDR, &opt, sizeof (opt));

		    //bind
		    if (bind(fds[nfds].fd, runp->ai_addr, runp->ai_addrlen) != 0)
		    {
		        if (errno != EADDRINUSE)
		            error(EXIT_FAILURE, errno, "bind");
		        close(fds[nfds].fd);
		    }
		    else
		    {
		        //listen for connections
		        int listenres;
		        if ((listenres = listen(fds[nfds].fd, BACKLOG)) != 0)
		        {
		            error(EXIT_FAILURE, errno, "listen");
		        }
		        ++nfds;
		    }
		}

		freeaddrinfo(result);

		printf("ready to accept connections...\n");

		socklen_t sockstor_len;
		pthread_t thread_id;
		pthread_attr_t attr;
		pthread_attr_init(&attr);

		//server loop for accepting connections
		while(1)
		{
		    int n = poll (fds, nfds, 1000);
		    if (n > 0)
		    {
		        int i;
		        for (i = 0; i < nfds; ++i)
		        {
		            if (fds[i].revents & POLLIN)
		            {
		                struct sockaddr_storage sockstor;
		                sockstor_len = sizeof(sockstor);

		                int fd = accept(fds[i].fd, (struct sockaddr *)
		                			&sockstor, &sockstor_len);

		                //arguments for thread
		                struct thread_args *args = malloc(sizeof
		                						(struct thread_args));
		                args->fd = fd;
		                args->sockstor = sockstor;
		                args->sockstor_len = sockstor_len;
		                args->root = root;

		                if (fd == -1)
		                {
		                    fprintf(stderr, "error on accept\n");
		                    exit(EXIT_FAILURE);
		                }
		                else
		                {
		                    //spawn a new thread for every new connection
		                    pthread_create(&thread_id, &attr, process, args);
		                }
		            }
		        }
		    }
		}
    } // non relay mode end

    pthread_mutex_destroy(&lock);

    return 0;
}

/*
 * Synthetic load request: runloop
 */
void runloop(int fd, char *version)
{
    char message[] = "starting loop for 15 seconds\n";
    time_t endtime = time(NULL) + 15;
    send_response_header(fd, version, "/runloop", strlen(message));
    writen(fd, message, strlen(message));
    while(time(NULL) < endtime);
}

/*
 * Synthetic load request: allocanon
 */
void allocanon(int fd, char *version)
{
    char *memory;
    size_t size = 64 * 1000000; //64 megabytes
    if ((memory = mmap(0, size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    {
        fprintf(stderr,"mmap error\n");
    }
    //use memset to make os actually allocate.
    memset(memory, 0, size);

    pthread_mutex_lock(&lock);
    memories[memory_index] = memory;
    memory_index++;
    pthread_mutex_unlock(&lock);

    //send message to fd
    char buf[100];
    sprintf(buf, "Allocated memory. \
            %d blocks of memory are currently allocated", memory_index);
    send_response_header(fd, version, "/allocanon", strlen(buf));
    writen(fd, buf, strlen(buf));
}

/*
 * Synthetic load request: freeanon
 */
void freeanon(int fd, char *version)
{
    size_t size = 64 * 1000000;
    pthread_mutex_lock(&lock);
    if (memory_index > 0)
        memory_index--;

    if (munmap(memories[memory_index] , size) < 0)
        fprintf(stderr,"munmap error\n");
    memories[memory_index] = NULL;
    pthread_mutex_unlock(&lock);

    char buf[100];
    sprintf(buf, "Deallocated memory. %d blocks left", memory_index);
    send_response_header(fd, version, "/freeanon", strlen(buf));
    writen(fd, buf, strlen(buf));
}

/*
 * File serving
 */
void serve_file(int fd, char *root, char *path, char *version)
{
    // /.. not allowed
    if (strstr(path, "..") != NULL)
        clienterror(fd, path, version,
                "404", "Not Found", "Could not find this file");

	// build path
    char actual_path[MAXLINE];
    if (root == NULL)
    {
        sprintf(actual_path, ".");
        sprintf(actual_path, "%s%s", actual_path, path + strlen("/files"));
    }
    else
    {
        sprintf(actual_path, "%s", root);
        sprintf(actual_path, "%s%s", actual_path, path + strlen("/files"));
    }

	//acquire file stat
    struct stat filestat;
    if (stat(actual_path, &filestat) < 0)
    {
        clienterror(fd, actual_path, version,
                "404", "Not Found", "Could not find this file");
        return;
    }

    int filefd;
    char *fp;
    if ((filefd = open(actual_path, O_RDONLY)) == -1)
    {
        //should not happen
        fprintf(stderr, "open error\n");
        return;
    }

	//allocate memory for file and send the file content
    if ((fp = mmap(0, filestat.st_size,
                    PROT_READ, MAP_PRIVATE, filefd, 0)) == MAP_FAILED)
    {
        //should not happen
        fprintf(stderr, "mmap error\n");
        return;
    }

    close(filefd);
    send_response_header(fd, version, actual_path, filestat.st_size);
    writen(fd, fp, filestat.st_size);

    if (munmap(fp, filestat.st_size) < 0)
        fprintf(stderr, "munmap error\n");
}

/*
 * Acquires and sends /meminfo for a system.
 */
void meminfo(int fd, char *value, char *version)
{
    char *meminfodest = "/proc/meminfo";

    int meminfofd;
    if ((meminfofd = open(meminfodest, O_RDONLY)) == -1)
    {
        printf("open error\n");
        return;
    }

    char meminfobuf[MAXBUF];
    char json[MAXBUF];

    //parse meminfo into json format
    meminfo_json(meminfofd, json);

    //build output
    if (strlen(value) > 0) //callback argument provided
    {
        sprintf(meminfobuf, "%s", value);
        insert(meminfobuf, strlen(meminfobuf), '(', strlen(value));
        sprintf(meminfobuf, "%s%s", meminfobuf, json);
        insert(meminfobuf, strlen(meminfobuf), ')', strlen(meminfobuf));
    }
    else
    {
        sprintf(meminfobuf, "%s", json);
    }

    //send header and content
    send_response_header(fd, version, meminfodest, strlen(meminfobuf));
    writen(fd, meminfobuf, strlen(meminfobuf));
    close(meminfofd);
}

/*
 * Acquires and sends /loadavg for a system.
 */
void loadavg(int fd, char *value, char *version)
{
    char *loadavgdest = "/proc/loadavg";

    int loadavgfd;
    if ((loadavgfd = open(loadavgdest, O_RDONLY)) == -1)
    {
        //this shouldn't happen
        printf("open error\n");
        return;
    }

    char loadavgbuf[MAXBUF];
    char json[MAXBUF];

    //parse loadavg into json format
    loadavg_json(loadavgfd, json);

    //build output
    if (strlen(value) > 0) //callback argument provided
    {
        sprintf(loadavgbuf, "%s", value);
        insert(loadavgbuf, strlen(loadavgbuf), '(', strlen(value));
        sprintf(loadavgbuf, "%s%s", loadavgbuf, json);
        insert(loadavgbuf, strlen(loadavgbuf), ')', strlen(loadavgbuf));
    }
    else
    {
        sprintf(loadavgbuf, "%s", json);
    }

    //send results
    send_response_header(fd, version, loadavgdest, strlen(loadavgbuf));
    writen(fd, loadavgbuf, strlen(loadavgbuf));
    close(loadavgfd);
}

/*
 * Handle requests made to the server.
 */
void handle_request(int fd, char *root)
{
    rio_t rio;
    char buf[MAXLINE];
    char cmd[MAXLINE], dest[MAXLINE], httpver[MAXLINE];
    char hostname[MAXLINE] = {0};

    Rio_readinitb(&rio, fd);
    int bytesread;
    int initline = 1;

    //persistent connections
    do
    {
    	//read given command line
		while ((bytesread = readlineb(&rio, buf, MAXLINE)) > 0)
		{
		    if (initline && sscanf(buf, "%s %s %s", cmd, dest, httpver) == 3)
		    {
		        initline = 0;
		    }

		    if (strcasecmp(httpver, "HTTP/1.1") == 0)
		    {
		        if (sscanf(hostname, "Host: %s", hostname) != 1)
		        {
		            //hostname required for 1.1
		        }
		    }

		    if (bytesread <= 2)
		    {
		        if ((strcmp(buf, "\r\n") || strcmp(buf, "\n")))
		        {
		            if (initline == 0)
		                break;
		        }
		    }

		}

		if (bytesread <= 0)
		    return;

		initline = 1;
		//respond only if there are messages sent
		if (bytesread > 0)
		{
		    char request[100] = {0};
		    char value[100] = {0};
		    parse_request(dest, request, value);

		    if (strcasecmp(cmd, "GET") == 0
		    		&& strcasecmp(request, "/loadavg") == 0)
		    {
		        loadavg(fd, value, httpver);
		    }
		    else if (strcasecmp(cmd, "GET") == 0
		    		&& strcasecmp(request, "/meminfo") == 0)
		    {
		        meminfo(fd, value, httpver);
		    }
		    else if (strcasecmp(cmd, "GET") == 0
		    		&& strcasecmp(request, "/runloop") == 0)
		    {
		        runloop(fd, httpver);
		    }
		    else if (strcasecmp(cmd, "GET") == 0
		    		&& strcasecmp(request, "/allocanon") == 0)
		    {
		        allocanon(fd, httpver);
		    }
		    else if (strcasecmp(cmd, "GET") == 0
		    		&& strcasecmp(request, "/freeanon") == 0)
		    {
		        freeanon(fd, httpver);
		    }
		    else if (strcasecmp(cmd, "GET") == 0
		    		&& strncasecmp(request, "/files", strlen("/files")) == 0)
		    {
		        serve_file(fd, root, request, httpver);
		    }
		    else
		    {
		        //check if file descriptor is still open
		        if (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
		        {
		            if (strcasecmp(cmd, "GET") != 0)
		            {
		                clienterror(fd, cmd, httpver, "501",
		                      "Not Implemented", "Method is not implemented");
		                break;
		            }

		            if (strcasecmp(request, "/loadavg") != 0
		            		|| strcasecmp(request, "/meminfo") != 0
		                    || strcasecmp(request, "/runloop") != 0)
		            {
		                clienterror(fd, request, httpver, "404",
		                        "Not Found", "Could not find the file");
		                break;
		            }
		        }
		    }
		}

    }
    while (strcasecmp(httpver, "HTTP/1.1") == 0); //for persistent connections

}

/*
 * thread routine
 */
void *process(void *arg)
{
    //detach the thread so that when terminated the kernel reaps automatically
    pthread_detach(pthread_self());
    //retrieve arguments
    struct thread_args *args = (struct thread_args *) arg;

    handle_request(args->fd, args->root);

    close(args->fd);
    free(arg);

    return NULL;
}

/*
 * Writes response header to the provided file descriptor
 */
void send_response_header(int fd, char *version, char *filename, int filesize)
{
    char buf[MAXBUF];
    char type[100];
    filetype(filename, type);
    /* Send response headers to client */
    sprintf(buf, "%s 200 OK\r\n", version);
    sprintf(buf, "%sServer: CS3214 Sysstatd Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, type);
    writen(fd, buf, strlen(buf));
}

/*
 * Determines the type of the file to be served. Minimum support.
 */
void filetype(char *path, char *type)
{
    if (strstr(path, ".html"))
        strcpy(type, "text/html");
    else if (strstr(path, ".js"))
        strcpy(type, "text/JavaScript");
    else if (strstr(path, ".css"))
        strcpy(type, "text/css");
    else
        strcpy(type, "text/plain");
}

/*
 * From tiny.c
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *version, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>CS3214 Sysstatd Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "%s %s %s\r\n", version, errnum, shortmsg);
    writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    writen(fd, buf, strlen(buf));
    writen(fd, body, strlen(body));
}
