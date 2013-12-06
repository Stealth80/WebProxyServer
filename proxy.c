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
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size, const char* cachedStatus);
int handle_request(int connfd, struct sockaddr_in *sockaddr);



struct cachePage {
	char cachedHostName[MAXLINE];
	char cachedPathName[MAXLINE];
	char filename[1024];
};

struct cachePage cachedPages[1024];
int cachedPageCount = 0;
int fileCount = 0;

int port;
rio_t rio;
size_t n;
int isCached = 0;
char buf[MAXLINE], uri[MAXLINE], version[MAXLINE], method[MAXLINE];
char hostname[MAXLINE];
char pathname[MAXLINE];
int serverfd;

const char* HOSTCACHED = "(HOSTNAME CACHED)";
const char* PAGECACHED = "(PAGE CACHED)";
const char* NOTFOUND = "(NOTFOUND)";
const char* NOTCACHED = "";

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{     
	int listenfd, connfd, clientlen; //listenfd for listening descriptor, connfd for connected descriptor
	struct sockaddr_in clientaddr;
	struct hostent *hp;	//pointer to DNS host entry
	char *haddrp;	//pointer to dotted decimal string

    /* Check arguments */
    if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }

	port = atoi(argv[1]);  //listens on port passed on the command line
	listenfd = Open_listenfd(port);   //listening descriptor

	while(1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);
		printf("server connected to %s (%s)\n", hp->h_name, haddrp);
		//client_port = ntohs(clientaddr.sin_port);

		Rio_readinitb(&rio, connfd); //connection to client for reading
		n = Rio_readlineb(&rio, buf, MAXLINE); //read from client
		sscanf(buf, "%s %s %s", method, uri, version);  //scan input from client and extract method, uri, and version

		if (strcmp(method, "GET") != 0)
		{
			printf("Invalid Method. \n");
		}

		parse_uri(uri, hostname, pathname, &port);  //call parse_uri to extract host name, path name, and port
		printf("method = %s, version = %s, uri: %s, hostname = %s, pathname = %s, port = %d", method, version, uri, hostname, pathname, port);
		if (fileCount == 1024) {
			fileCount = 0;
		}
					
		
		

		//check for cache
		if (hostname == NULL) {
			//log error
		}
		else {
			if ((serverfd = Open_clientfd(hostname, port)) < 0)
			{
				printf("%d ", serverfd);

			}
			else { //connection was good
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

	

	
	//check URL against cached URL list
	if(isCached) //if cached already
	{
		
	}
	else //if not cached already
	{
		printf("File was not cached\n");
		strcpy(cachedPages[fileCount].cachedHostName, hostname);
		strcpy(cachedPages[fileCount].cachedPathName, pathname);
		printf("%s, %s, %d\n", hostname, pathname, port);
		//if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) //open connection to end server
		
		
		Rio_writen(serverfd, buf, n);
		Rio_writen(serverfd, "\n", 1);

		Rio_readinitb(&rio, serverfd);
		printf("Data received from server\n");
		//strcat(buf, "\n"); //add end line
		//Rio_writen(clientfd, buf, bufSize+1);  //send client request to server

		//printf("type:");
		//fflush(stdout);
		//while(Fgets(buf, MAXLINE, stdin) ! = NULL) { //read input line from client
		//	size += strlen(buf);
		//	Rio_writen(clientfd, buf, strlen(buf)); //send line to server
		//	Rio_readlineb(&rio, buf, MAXLINE);  //receive line back from server
		//}
		//fileCount hit end of array;

		char fileCountAsChar[4];
		sprintf(fileCountAsChar, "%d", fileCount);
		printf("File name is %s\n", fileCountAsChar);
		cachedfp = fopen(fileCountAsChar, "w");
		while ((m = Rio_readn(serverfd, msg, MAXLINE)) > 0) {
			printf("trying to write to file\n");
			fprintf(cachedfp,"%s", msg);
			printf("trying to write to client\n");
			Rio_writen(connfd, msg, m);
			/*sum the total number of bytes written */
			bufSize += m;
		}
		fclose(cachedfp);
		strcpy(cachedPages[fileCount].filename, fileCountAsChar);



		//write log entry to log file
		format_log_entry(logstring, sockaddr, uri, bufSize, NOTCACHED);
		printf("%s\n",logstring);
		fp = fopen("proxy.log", "a");
		if (!fp) {
			printf("Log not written!");
		}
		else {
			fprintf(fp, "%s\n", logstring);
			fclose(fp);
		}

		Close(serverfd);
	}
	return 0;
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
		      char *uri, int size, const char* cachedStatus)
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
	if(size < 1)
	{
		sprintf(logstring, "%s: %d.%d.%d.%d %s (NOTFOUND)", time_str, a, b, c, d, uri);
	}
	else
	{
		sprintf(logstring, "%s: %d.%d.%d.%d %s %d %s", time_str, a, b, c, d, uri, size, cachedStatus);
	}
}


