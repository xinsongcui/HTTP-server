
const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <dlfcn.h>

typedef void (*httprunfunc)(int ssock, const char* querystring);


int QueueLength = 5;
int requests = 0;
int _GET = 0;
int _POST = 0;
pthread_mutex_t mutex;
pthread_attr_t attr;
int masterSocket;
double minimumTime = 100000000.0;
double maximumTime = 0.0;
char slow [256];
char fast [256];
char host [1024];
const char* protocal = "HTTP/1.1";
const char* serverType = "lab5";
const char* sp = "\040";
const char* tr = "</tr>";
const char* crlf = "\015\012";
const char* notFound = "File Not Found\n";

char * nbsp = "</td><td>&nbsp;";
char * alignRight = " </td><td align=\"right\">";

const char * icon_unknown = "../icons/unknown.gif";
const char * icon_menu = "../icons/menu.gif";
const char * icon_back = "../icons/back.gif";
const char * icon_chat = "../icons/image.gif";

const char * header = 
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n"
"<html>\n"
" <head>\n"  
"  <title>Index of /homes/cs252/lab5-http-server/lab5-src/http-root-dir/htdocs/dir1</title>\n"
" </head>\n"
" <body>\n"
"<h1>Index of /homes/cs252/lab5-http-server/lab5-src/http-root-dir/htdocs/dir1</h1>\n"
"  <table>\n"
"<tr><th valign=\"top\"><img src=\"../icons/blank.gif\" alt=\"[ICO]\"></th>";

const char * NA = "<th><a href=\"?C=N;O=A\">Name</a></th>";
const char * ND = "<th><a href=\"?C=N;O=D\">Name</a></th>";
const char * MA = "<th><a href=\"?C=M;O=A\">Last modified</a></th>";
const char * MD = "<th><a href=\"?C=M;O=D\">Last modified</a></th>";
const char * SA = "<th><a href=\"?C=S;O=A\">Size</a></th>";
const char * SD = "<th><a href=\"?C=S;O=D\">Size</a></th>";
const char * DA = "<th><a href=\"?C=D;O=A\">Description</a></th>";
const char * DD = "<th><a href=\"?C=D;O=D\">Description</a></th>";
const char * colspan = "<tr><th colspan=\"5\"><hr></th></tr>\n";

const char * valign = "<tr><td valign=\"top\"><img src=\"";
const char * ALT= "\" alt=\"[";
const char * href = "]\"></td><td><a href=\"";
const char * greater = "\">";
const char * dasha = "</a>     ";
const char * tdnbsp = " </td><td>&nbsp;</td></tr>\n";

const char * end =  
"</table>\n"
"<address>Apache/2.4.18 (Ubuntu) Server at www.cs.purdue.edu Port 443</address>\n"
"</body></html>\n";

const char * Parent = "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href="">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>\n" ;

//const char * Parent = "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href=\"/u/data/u97/cui102/cs252/lab5-src/http-root-dir/htdocs/\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>\n" ;

const char * name = "Xinsong Cui";
const char * uptime = "";

// Processes time request
void wirteTo (int socket, char * path, char * name, char * fileType, char * parent);
void processTimeRequest( int socket );
void processRequestThread(int socket );
void poolSlave(int socket);

extern "C" void killzombie(int sig){
  while(waitpid(-1, NULL, WNOHANG) > 0);
  close(masterSocket);
  exit(1);
}

int
main( int argc, char ** argv )
{
  time_t  t;
  time(&t);
  uptime = ctime(&t);

  int port = 0;
  int mode = 0;
  if(argc == 1){
    port = 53123;
  } 
  if(argc == 2){
    port = atoi(argv[1]);
  }
  if(argc == 3){
    port = atoi(argv[2]);

    if(!strcmp(argv[1], "-f")){
      mode = 1; 
    }
    else if(!strcmp(argv[1], "-t")){
      mode = 2;
    }
    else if(!strcmp(argv[1], "-p")){
      mode = 3;
    }
    else{
      fprintf(stderr, "%s", usage);
      exit( -1 );
    }
  }
  if(argc > 3 || port < 1024 || port > 65536) {
    fprintf(stderr, "%s", usage);
    exit( -1 );
}
  // Get the port from the arguments
  //int port = atoi( argv[1] );
  //printf("%d\n",port);
  struct sigaction signalAction;
    
  signalAction.sa_handler = killzombie;
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;
  int err1 = sigaction(SIGINT, &signalAction, NULL);
  //int err1 = sigaction(SIGCHLD, &signalAction, NULL);
  if (err1){
      perror("sigaction");
      exit(-1);
  }

  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }
  if (mode == 3) {
    pthread_t tid[5];
    
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    pthread_mutex_init(&mutex, NULL);

    for(int i = 0; i< 5; i++) {
      pthread_create(&tid[i], &attr, (void *(*)(void *)) poolSlave, (void *)(intptr_t) masterSocket);
    }
    pthread_join(tid[0], NULL);
   }
   while ( 1 ) {
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    if( mode == 0 ){
      if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
      }
      // Process request.
      processTimeRequest( slaveSocket );

      // Close socket
      close( slaveSocket );
    }
    else if(mode == 1){
      int pid = fork();
      if(pid == 0){
         processTimeRequest( slaveSocket );
         close( slaveSocket );
	 exit(EXIT_SUCCESS);
      }
      close(slaveSocket);
    }
    else if(mode == 2){
      pthread_t tid;
      //pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
      pthread_create(&tid, &attr, (void * (*)(void *))processRequestThread, (void *)(intptr_t)slaveSocket);
      pthread_join(tid, NULL);
    }

  }
}

int endsWith(char* s1, char* s2){
	int i = strlen(s1)-1;
	int j = strlen(s2)-1;
	int check = 1;
	while(j>=0){
		if(s1[i--] != s2[j--]){
			check = 0;
		}
	}
	return check;
}

int sortNameA (const void * name1, const void * name2)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;

	int s = strcmp(n1, n2);

	return s;
}

int sortNameD (const void * name2, const void * name1)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;
	
	int s = strcmp(n1, n2);

	return s;
}

int sortTimeA (const void * name1, const void * name2)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;

	struct stat n1stat, n2stat;

	stat(n1, &n1stat);
	stat(n2, &n2stat);

	int s = difftime(n1stat.st_mtime, n2stat.st_mtime);

	return s;

}

int sortTimeD (const void * name2, const void * name1)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;

	struct stat n1stat, n2stat;

	stat(n1, &n1stat);
	stat(n2, &n2stat);
		
	int s = difftime(n1stat.st_mtime, n2stat.st_mtime);

	return s;

}
int sortSizeA (const void * name1, const void * name2)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;

	struct stat n1stat, n2stat;

	stat(n1, &n1stat);
	stat(n2, &n2stat);
	
	int s1 = n1stat.st_size;
	int s2 = n2stat.st_size;

	if(strstr(n1, "subdir1") ) return -10000;
	if(strstr(n2, "subdir1") ) return 10000;

	return s1 - s2;
}

int sortSizeD (const void * name2, const void * name1)
{
	const char * n1 = *(const char **)name1;
	const char * n2 = *(const char **)name2;

	struct stat n1stat, n2stat;

	stat(n1, &n1stat);
	stat(n2, &n2stat);

	int s1 = n1stat.st_size;
	int s2 = n2stat.st_size;

	if(strstr(n1, "subdir1") ) return -10000;
	if(strstr(n2, "subdir1") ) return 10000;

	return s1 - s2;
}

void
poolSlave(int socket) {
    while(1) {
        struct sockaddr_in clientIPAddress;
        int alen = sizeof(clientIPAddress);
        pthread_mutex_lock(&mutex);

        int slaveSocket = accept(socket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);
        pthread_mutex_unlock(&mutex);
	
        processTimeRequest(slaveSocket);
	pthread_attr_destroy(&attr);
    }
}

void
processRequestThread(int socket){
	processTimeRequest(socket);
	pthread_attr_destroy(&attr);
	close(socket);

}

void
processTimeRequest( int socket )
{
	int n;	
	int c = 0 ;
	int error = 0;
   	int length = 0;
	int gotGet = 0;
	int keycheck = 0;
	char currString[1024];
	char * docPath;
	unsigned char newChar = ' ';
	unsigned char nnewChar = ' ';
	unsigned char oldChar = ' ';
	unsigned char ooldChar = ' ';
	requests ++;
	clock_t start_t, end_t, current_t;
	start_t = clock();

	//Read the HTTP header
   	while(n = read(socket, &nnewChar, sizeof(newChar)>0)){
      		if(newChar == '\015' && ooldChar == '\015' && oldChar == '\012' && nnewChar == '\012'){
			break;
		}
      		if(nnewChar == ' '){
			c++;
			currString[length] = '\0';
			length = 0;
       			if(c == 2){
            			docPath = strdup(currString);
			}
			
      		}
		else{
			ooldChar = oldChar;
			oldChar = newChar;
			newChar = nnewChar;
			currString[length++] = nnewChar;
		}
   	}

	//Map the document path to the real file
	char cwd[256] = {0};
	getcwd(cwd, sizeof(cwd));
	int lcheck = strlen(cwd) + strlen("/http-root-dir");
	if(!strncmp(docPath, "/icons", 6)){
		strcat(cwd, "/http-root-dir");
		strcat(cwd, docPath);
	} 
	else if(!strncmp(docPath, "/htdocs", 7)){
		strcat(cwd, "/http-root-dir/");
		strcat(cwd, docPath);
	} 

	else if(!strcmp(docPath, "/")){
		strcat(cwd, "/http-root-dir/htdocs/index.html");
	}
	else{
		strcat(cwd, "/http-root-dir/htdocs");
		strcat(cwd, docPath); 
	}

	//Expand fileapth
	if(strstr(docPath, "..")){
		char actualpath[1024];
		char* ptr;
		ptr = realpath(docPath, actualpath);
		if(strlen(actualpath) < lcheck){
			error = 1;
		}
		else{
			strcpy(cwd, actualpath);
		}
	}

	//Log file
	host[1023] = '\0';
	gethostname(host, 1023);
	FILE * fd = fopen("/u/data/u97/cui102/cs252/lab5-src/http-root-dir/logs", "a");
	fwrite(host, sizeof(char), strlen(host), fd);
	fwrite(":", sizeof(char), 1, fd);
	fwrite(docPath, sizeof(char), strlen(docPath), fd);
	fwrite("\n", sizeof(char), 1, fd);
	fclose(fd);

	//Determine content type
	char contentType[32] = {0};
	int openType = 1;
	char sortmode = 0;
	char sortorder = 0;
	char* start;
	int len = strlen(cwd);
	printf( cwd);
	printf( "\n");
	if(cwd[len-8]=='?' && cwd[len-7] == 'C' && cwd[len-6] == '=' && cwd[len-4] == ';'){
		sortmode = cwd[len-5];
		sortorder = cwd[len-1];
		cwd[len-8] = '\0';
		docPath[strlen(docPath)-8] = 0;
		openType = 3;
	}
	if(cwd[len-9]=='?' && cwd[len-8] == 'C' && cwd[len-7] == '=' && cwd[len-5] == ';'){
		sortmode = cwd[len-6];
		sortorder = cwd[len-2];
		cwd[len-9] = '\0';
		docPath[strlen(docPath)-9] = 0;
		openType = 3;
	}
	DIR * dir = opendir(cwd);
	if(dir != NULL){
		//Browsing dir
		openType = 3;
	}
	else if(endsWith(cwd, "stats") == 1 || endsWith(cwd, "stats/") == 1){
		//Statistics
		openType = 4;
	}
	else if(endsWith(cwd, "logs") == 1 || endsWith(cwd, "logs/") == 1){
		//Log pages
		openType = 5;
	}
	else if(strstr(docPath, "cgi-bin") || strstr(docPath, "cgi-bin/")){
		//cgi-bin
		openType = 6;
	}
	else if(endsWith(cwd, ".html") == 1 || endsWith(cwd, ".html/") == 1){
		//html
		strcpy(contentType, "text/html");
	}
	else if(endsWith(cwd, ".gif") == 1 || endsWith(cwd, ".gif/") == 1){
		//gif
		strcpy(contentType, "image/gif");
		openType = 2;
	}
	else if(endsWith(cwd, ".jpg") == 1 || endsWith(cwd, ".jpg/") == 1){
		//jpg
		strcpy(contentType, "image/jpeg");
		openType = 2;
	}
	else if(endsWith(cwd, ".svg") == 1 || endsWith(cwd, ".svg/") == 1){
		//svg
		strcpy(contentType, "image/svg+xml");
		openType = 2;
	}
	else{
		//plain
		strcpy(contentType, "text/plain");
	}

	
	FILE * doc;
	if(openType == 6){
		if(strstr(docPath, ".so")){
			write(socket, protocal, strlen(protocal));
			write(socket, " ", 1);
			write(socket, "200", 3);
			write(socket, " ", 1);
     			write(socket, "Document", 8);
			write(socket, " ", 1);
      			write(socket, "follows", 7);
      			write(socket, crlf, 2);
			write(socket, "Server:", 7);
			write(socket, " ", 1);
			write(socket, serverType, strlen(serverType));
			write(socket, crlf, 2);
			write(socket, "Content-type:", 13);
			write(socket, " ", 1);
    			write(socket, "text/html", 10);
			write(socket, crlf, 2);
			write(socket, crlf, 2);

			void * lib;
			if(strstr(docPath, "hello.so")){
				lib = dlopen( "./hello.so", RTLD_LAZY );
			}
			if(strstr(docPath, "jj-mode.so")){
				lib = dlopen( "./jj-mode.so", RTLD_LAZY );
			}
  			if ( lib == NULL ) {
    				fprintf( stderr, "not found\n");
    				perror( "dlopen");
    				exit(1);
 			 }

  			httprunfunc hello_httprun;
  			hello_httprun = (httprunfunc) dlsym( lib, "httprun");

 		 	if ( hello_httprun == NULL ) {
   				perror( "dlsym: httprun not found:");
    				exit(1);
		  	}
		 	hello_httprun( socket, "a=b&c=d");
		}
		else{
			write(socket, protocal, strlen(protocal));
			write(socket, " ", 1);

			start = strchr(docPath, '?');

			if (start) {
				*start = '\0';
				printf(start);
				printf("\n");
			}
			
			else {
				char ** args = (char **)malloc(sizeof(char *) * 2);
				args[0] = (char *)malloc(sizeof(char) * (strlen(docPath)+1));	
				args[1] = NULL;
	
				start = strchr(docPath, '?');

				if(start){
					start++;
					strcpy(args[0], start);
				}
				else{
					for(int i=0; i<strlen(docPath); i++){
						args[0][i] = '\0';
					}
				}

				write(socket, protocal, strlen(protocal));
				write(socket, " ", 1);
				write(socket, "200", 3);
				write(socket, " ", 1);
     				write(socket, "Document", 8);
				write(socket, " ", 1);
      				write(socket, "follows", 7);
      				write(socket, crlf, 2);
				write(socket, "Server:", 7);
				write(socket, " ", 1);
				write(socket, serverType, strlen(serverType));
				if(strstr(cwd, "jj")){
					write(socket, crlf, 2);
					write(socket, "Content-type:", 13);
					write(socket, " ", 1);
    					write(socket, "text/html", 10);
				}
				write(socket, crlf, 2);
				write(socket, crlf, 2);
				
				int STDOUT = dup(1);
				dup2(socket, 1);
				close(socket);

				pid_t ret = fork();
				if (ret == 0) {
					setenv("REQUEST_METHOD","GET", 1);
					setenv("QUERY_STRING",args[0], 1);
					printf(args[0]);
					printf("\n");
					execvp(cwd, args);
						//printf("ERROR\n");
					//}
					//free(args[0]);
					//free(args);
					exit(2);
				}
				dup2(STDOUT,1);
				close(STDOUT);
				
			}
		}	
	}
	else if(openType == 3){	
		strcpy(contentType, "text/html");
		struct dirent * ent;
		char ** content = (char **)malloc(sizeof(char *) * 1024);
		int count = 0;	
		while((ent = readdir(dir))!= NULL){
			if(strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")){
				content[count] = (char *)malloc(sizeof(char) * 128);
					content[count][0] = '\0'; 
					strcat(content[count], cwd);
					if (!endsWith(cwd, "/")) {
						strcat(content[count], "/");
					}
					strcat(content[count], ent -> d_name);
					count++;
			}
		}
	
		write(socket, protocal, strlen(protocal));
		write(socket, " ", 1); 
		write(socket, "200", 3);
		write(socket, " ", 1);
     		write(socket, "Document", 8);
		write(socket, " ", 1);
      		write(socket, "follows", 7);
      		write(socket, crlf, 2);
		if(contentType !=NULL){
		write(socket, "Server:", 7);
		write(socket, " ", 1);
      		write(socket, serverType, strlen(serverType));
      		write(socket, crlf, 2);
      		write(socket, "Content-type:", 13);
		write(socket, " ", 1);
      		write(socket, contentType, strlen(contentType));
      		write(socket, crlf, 2);
		write(socket, crlf, 2);
		}
		write(socket, header, strlen(header));
		if(sortmode == 0 || sortorder == 0){
			write (socket, ND, strlen(ND));
			write (socket, MD, strlen(MD));
			write (socket, SD, strlen(SD));
			write (socket, DD, strlen(DD));
		}
		if(sortmode == 'S'){
			if(sortorder == 'A'){
				qsort(content, count, sizeof(char *), sortSizeA);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
			if(sortorder == 'D'){
				qsort(content, count, sizeof(char *), sortSizeD);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SA, strlen(SA));
				write (socket, DD, strlen(DD));
			}
		}	
		if(sortmode == 'M'){
			if(sortorder == 'A'){
				qsort(content, count, sizeof(char *), sortTimeA);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
			if(sortorder == 'D'){
				qsort(content, count, sizeof(char *), sortTimeD);	
				write (socket, ND, strlen(ND));
				write (socket, MA, strlen(MA));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
		}
		if(sortmode == 'N'){
			if(sortorder == 'A'){
				qsort(content, count, sizeof(char *), sortNameA);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
			if(sortorder == 'D'){
				qsort(content, count, sizeof(char *), sortNameD);
				write (socket, NA, strlen(NA));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
		}
		if(sortmode == 'D'){
			if(sortorder == 'A'){
				qsort(content, count, sizeof(char *), sortNameA);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DD, strlen(DD));
			}
			if(sortorder == 'D'){
				qsort(content, count, sizeof(char *), sortNameD);
				write (socket, ND, strlen(ND));
				write (socket, MD, strlen(MD));
				write (socket, SD, strlen(SD));
				write (socket, DA, strlen(DA));
			}
		}
		write(socket, tr, strlen(tr));
		write(socket, colspan, strlen(colspan));
		write(socket, Parent, strlen(Parent));
		
		for (int i = 0; i < count; i++) {
			char * name = (char *)malloc(sizeof(char ) * strlen(content[i]));
			char * fileType = "   ";
			int fcount = 0;
			for (int j = (strlen(content[i]) - 1); j >= 0; j --) {
				if (content[i][j] == '/') {
					fcount = j;
					break;
				}
			}
			for (int j = fcount + 1; j < strlen(content[i]); j ++) {
				name[j - fcount - 1] = content[i][j];
				if (j == strlen(content[i]) - 1) {
					name[j - fcount] = '\0';
				}
			}

			if (opendir(content[i]) != NULL) fileType = "DIR";

			char * m_time = (char *)malloc(sizeof(char) * 100);
			int s = 0;
			struct stat names;
			stat(content[i], &names);
			struct tm * timeInfo;
			timeInfo = localtime(&(names.st_mtime));
			strftime(m_time, 20, "%F %H:%M", timeInfo);
			s = names.st_size;

			if (!strcmp(name, "chat.gif")) {
				write(socket, valign, strlen(valign));
				write(socket, icon_chat, strlen(icon_chat));
			}
			else if (!strcmp(fileType, "DIR")) {
				write(socket, valign, strlen(valign));
				write(socket, icon_menu, strlen(icon_menu));
			}		
			else {
				write(socket, valign, strlen(valign));
				write(socket, icon_unknown, strlen(icon_unknown));
			}
	
			write(socket, ALT, strlen(ALT));
			write(socket, fileType, strlen(fileType));
			write(socket, href, strlen(href));

			if (!endsWith(docPath, "/")) {
				write(socket, docPath, strlen(docPath));
				write(socket, "/", 1);
				write(socket, name, strlen(name));
			}
			else {
				write(socket, name, strlen(name));
			}
	
			write(socket, greater, strlen(greater));
			write(socket, name, strlen(name));
			write(socket, dasha, strlen(dasha));
			write(socket, alignRight, strlen(alignRight));
			write(socket, m_time, strlen(m_time));
			write(socket,alignRight, strlen(alignRight));

			if (strcmp(fileType, "DIR")) {
				char t[16];
				snprintf(t, 16, "%d", s);
				write(socket, t, strlen(t));
			}

			if(!strcmp(fileType, "DIR")) {
				write(socket, "-", 1);
			}

			write (socket, tdnbsp, strlen(tdnbsp));
			free(m_time);
			free(name);
		}
		write(socket, colspan, strlen(colspan));
		write(socket, end, strlen(end));	
	}

	else if(openType == 4){
		char requestnum[256];
		char mintime[256];
		char maxtime[256];
		sprintf(requestnum, "%d", requests);
		sprintf(mintime, "%f", minimumTime);
		sprintf(maxtime, "%f", maximumTime);
		
		write(socket, protocal, strlen(protocal));
		write(socket, " ", 1);
		write(socket, "200", 3);
		write(socket, " ", 1);
     		write(socket, "Document", 8);
		write(socket, " ", 1);
      		write(socket, "follows", 7);
      		write(socket, crlf, 2);
		write(socket, "Server:", 7);
		write(socket, " ", 1);
		write(socket, serverType, strlen(serverType));
		write(socket, crlf, 2);
		write(socket, "Content-type:", 13);
		write(socket, " ", 1);
    		write(socket, "text/plain", 10);
		write(socket, crlf, 2);
		write(socket, crlf, 2);

		write(socket, "Student Name: ", 14);
		write(socket, name, strlen(name));
		write(socket, "\n", 1);
		write(socket, "Sever Uptime: ", 14);
		write(socket, uptime, strlen(uptime));
		write(socket, "Number of Requests: ", 20); 
		write(socket, requestnum, strlen(requestnum));
		write(socket, "\n", 1);
		write(socket, "Min Service Time: ", 18); 
		write(socket, mintime, strlen(mintime));
		write(socket, "\n", 1);
		write(socket, "Slowest URL Request: ",21);
		write(socket , slow, strlen(slow));
		write(socket, "\n", 1);
		write(socket, "Max Service Time: ", 18); 
		write(socket, maxtime, strlen(maxtime));
		write(socket, "\n", 1);
		write(socket, "Fastest URL Request: ",21);
		write(socket , fast, strlen(fast));
	}
	else if(openType == 5){
		FILE * fd = fopen("/u/data/u97/cui102/cs252/lab5-src/http-root-dir/logs", "r");
		char * line = NULL;
		size_t len = 0;
		ssize_t read;
		write(socket, protocal, strlen(protocal));
		write(socket, " ", 1);
		write(socket, "200", 3);
		write(socket, " ", 1);
     		write(socket, "Document", 8);
		write(socket, " ", 1);
      		write(socket, "follows", 7);
      		write(socket, crlf, 2);
		write(socket, "Server:", 7);
		write(socket, " ", 1);
		write(socket, serverType, strlen(serverType));
		write(socket, crlf, 2);
		write(socket, "Content-type:", 13);
		write(socket, " ", 1);
    		write(socket, "text/plain", 10);
		write(socket, crlf, 2);
		write(socket, crlf, 2);
		while((read = getline(&line, &len, fd)) != -1){
			write(socket, line, strlen(line));
		}
		fclose(fd);

	}
	else{
		if(openType == 1){
			doc = fopen(cwd, "r");
		}
		if(openType == 2){
			doc = fopen(cwd, "rb");
		}
		if(doc == NULL || error == 1 ){
			write(socket, protocal, strlen(protocal));
			write(socket, " ", 1); 
    			write(socket, "404 File Not Found", 18);
    			write(socket, crlf, 2);
    			write(socket, "Server:", 7);
			write(socket, " ", 1);
    			write(socket, serverType, strlen(serverType));
    			write(socket, crlf, 2);
    			write(socket, "Content-type:", 13);
			write(socket, " ", 1);
    			write(socket, contentType, strlen(contentType));
    			write(socket, crlf, 2);
    			write(socket, crlf, 2);
			write(socket, notFound, strlen(notFound));
		}
		else if(doc){
			write(socket, protocal, strlen(protocal));
      			write(socket, "200", 3);
			write(socket, " ", 1);
     			write(socket, "Document", 8);
			write(socket, " ", 1);
      			write(socket, "follows", 7);
      			write(socket, crlf, 2);
      			write(socket, "Server:", 7);
			write(socket, " ", 1);
      			write(socket, serverType, strlen(serverType));
      			write(socket, crlf, 2);
      			write(socket, "Content-type:", 13);
			write(socket, " ", 1);
      			write(socket, contentType, strlen(contentType));
      			write(socket, crlf, 2);
			write(socket, crlf, 2);
	
			int count = 0;
			char c;
			while(count = read(fileno(doc), &c, sizeof(c))){
				if(write(socket, &c, sizeof(c))!=count){
					perror("wirte");
				}
			}	
			fclose(doc);
			close(socket);
		}
		free(docPath);
	}

	end_t = clock();
	if ((double)(end_t-start_t) < minimumTime) {
		minimumTime = (double)(end_t - start_t);
		strcpy(fast, docPath);
		fast[strlen(docPath)] = '\0';
	}
	else if ((double)(end_t-start_t) > maximumTime) {
		maximumTime = (double)(end_t - start_t);
		strcpy(slow, docPath);
		slow[strlen(docPath)] = '\0';
	}
}

