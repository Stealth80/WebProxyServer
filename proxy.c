/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS: 
 *     Nick Hollis, nick.hollis@uky.edu 
 *     Josh Tuschl, student2@cs.uky.edu 
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"
#include "stdio.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size, char* cachedStatus);
int handle_request(int connfd, struct sockaddr_in *sockaddr);
int checkIfPageCached();
void sigchld_handler(int sig);
int Openclientfd(char *hostname, int port);
int openclientfd(char *hostname, int port);


struct cachePage {
	char cachedHostName[MAXLINE];
	char cachedPathName[MAXLINE];
	char filename[1024];
};

struct cachePage cachedPages[1024];
int fileCount = 0;
int isPageCached = -1;
int isIPCached = -1;

int port;
rio_t rio;
size_t n;
char buf[MAXLINE], uri[MAXLINE], version[MAXLINE], method[MAXLINE];
char hostname[MAXLINE];
char pathname[MAXLINE];
int serverfd;
char status[36] = "";

const char* HOSTCACHED = "(HOSTNAME CACHED)";
const char* PAGECACHED = "(PAGE CACHED)";
const char* NOTFOUND = "(NOTFOUND)";
const char* NOTCACHED = "(ADDED TO CACHE)";

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{     
	int listenfd, connfd, clientlen; //listenfd for listening descriptor, connfd for connected descriptor
	struct sockaddr_in clientaddr;

    /* Check arguments */
    if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }

	port = atoi(argv[1]);  //listens on port passed on the command line
	Signal(SIGCHLD, sigchld_handler);
	listenfd = Open_listenfd(port);   //listening descriptor

	while(1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

		Rio_readinitb(&rio, connfd); //connection to client for reading, creates a read buffer
		n = Rio_readlineb(&rio, buf, MAXLINE); //read from client
		sscanf(buf, "%s %s %s", method, uri, version);  //scan input from client and extract method, uri, and version

		if (strcmp(method, "GET") != 0)
		{
			printf("%s is not a valid method. \n", method);
		}
		else {
			parse_uri(uri, hostname, pathname, &port);  //call parse_uri to extract host name, path name, and port
			//printf("method = %s, version = %s, uri: %s, hostname = %s, pathname = %s, port = %d\n", method, version, uri, hostname, pathname, port);
			if (fileCount == 1024) {
				fileCount = 0;
			}

			//check for cache
			isPageCached = checkIfPageCached();

			if (hostname == NULL) {
				printf("Invalid host name.\n");
			}
			else {
				if ((serverfd = Openclientfd(hostname, port)) < 0)
				{
					printf("%d ", serverfd);

				}
				else { //connection was good
					char fileCountAsChar[4];
					sprintf(fileCountAsChar, "%d", fileCount);
					strcpy(cachedPages[fileCount].cachedHostName, hostname);
					strcpy(cachedPages[fileCount].cachedPathName, pathname);
					strcpy(cachedPages[fileCount].filename, fileCountAsChar);
					if (fork() == 0) { //if child
						Close(listenfd); //close listen socket
					
						if (handle_request(connfd, &clientaddr) < 0) //handle request
						{
							printf("Error Handling Request");
						}
						exit(0);  //on exit will close remaining fd and child ends
					}
					else  //if parent
					{
						Close(connfd);  //close connection fd
						Close(serverfd);
						fileCount++;
					}
				}
			}
		}
	}

    exit(0);   //should never get here
}


int handle_request(int connfd, struct sockaddr_in *sockaddr)
{
	
	int bufSize=0;
	char logstring[MAXLINE];
	char msg[MAXLINE];
	size_t m;
	FILE *fp;
	FILE *cachedfp;
	int cachedfd;

	
	//check URL against cached URL list
	if(isPageCached > -1) //if cached already
	{
		char fileLocationAsChar[4];
		sprintf(fileLocationAsChar, "%d", isPageCached);
		cachedfd = open(fileLocationAsChar, O_RDONLY);
		if (cachedfd > -1)
		{
			if (strlen(status) == 0) {
				strcpy(status, PAGECACHED);
			}
			else {
				strcat(status, PAGECACHED);
			}
			printf("File %s was output from cache\n", fileLocationAsChar);
			dup2(cachedfd, serverfd);
		}
	}
	else //if not cached already
	{
		char serverRequestLine1[MAXLINE];
		char serverRequestLine2[MAXLINE];

		sprintf(serverRequestLine1, "GET /%s HTTP/1.1\n", pathname);
		sprintf(serverRequestLine2, "Host:%s\n", hostname);

		Rio_writen(serverfd, serverRequestLine1, strlen(serverRequestLine1));
		Rio_writen(serverfd, serverRequestLine2, strlen(serverRequestLine2));
		Rio_writen(serverfd, "\n", 1);

		Rio_readinitb(&rio, serverfd);
		printf("Data received from server\n");

		char fileCountAsChar[4];
		sprintf(fileCountAsChar, "%d", fileCount);
		cachedfp = fopen(fileCountAsChar, "w");
		if (strlen(status) == 0) {
			strcpy(status, NOTCACHED);
		}
		else {
			strcat(status, NOTCACHED);
		}

	}
		
	while ((m = Rio_readn(serverfd, msg, MAXLINE)) > 0) {
		if (isPageCached < 0) {
			//printf("trying to write to file\n");
			fprintf(cachedfp, "%s", msg);
		}
		Rio_writen(connfd, msg, m);
		/*sum the total number of bytes written */
		bufSize += m;
	}

	if (isPageCached < 0) {
		fclose(cachedfp);
	}
	else
	{
		Close(cachedfd);
	}


	//write log entry to log file
	format_log_entry(logstring, sockaddr, uri, bufSize, status);
	//printf("%s\n",logstring);
	fp = fopen("proxy.log", "a");
	if (!fp) {
		printf("Log not written!");
	}
	else {
		fprintf(fp, "%s\n", logstring);
		fclose(fp);
	}

	Close(serverfd);
	
	return 0;
}

int checkIfPageCached() {
	int index;
	for (index = 0; index < fileCount; index++) {
		if (!strcmp(cachedPages[index].cachedHostName, hostname)) {
			if (!strcmp(cachedPages[index].cachedPathName, pathname)) {
				return index;
			}
		}
	}	
	return -1;
}

int checkIfIPCcached() {
	return -1;
}

void sigchld_handler(int sig) {
	while (waitpid(-1, 0, WNOHANG) > 0)
		;
	return;
}



int openclientfd(char *hostname, int port)
{
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1; /* check errno for cause of error */

	/* Fill in the server's IP address and port */
	if ((hp = gethostbyname(hostname)) == NULL)
		return -2; /* check h_errno for cause of error */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0],
		(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	/* Establish a connection with the server */
	if (connect(clientfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;
	return clientfd;
}

int Openclientfd(char *hostname, int port)
{
	int rc;

	if ((rc = open_clientfd(hostname, port)) < 0) {
		if (rc == -1)
			unix_error("Open_clientfd Unix error");
		else
			dns_error("Open_clientfd DNS error");
	}
	return rc;
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size, char* cachedStatus)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */

	sprintf(logstring, "%s: %d.%d.%d.%d %s %d %s", time_str, a, b, c, d, uri, size, cachedStatus);
	
}


