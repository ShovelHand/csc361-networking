/* Alexander Carmichael v00174746's solution to p1 csc361 fall 2014
*  This solution is built on the back of "sws_select.c" which was provided as a lab example, and as such not all code is written by me. 
*  I have made comments pointing out which code was borrowed 
*  This is probably some of the messiest code I've ever written, and it's a pain to try and read.
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>  //be able to keep track of when stuff happened.

#define MAXBUFLEN 256
//globally accessable string. determines where files served from
char cwd[1024];  //an arbitrarily large size
char log_string[1024];
int code = -1;		//return code (200, 400, 404)
//time struct
 time_t rawtime;
  struct tm * timeinfo;

//request struct. probably not too clever to make it global, but it sure is convenient
struct request {
	int client_socket;
	struct sockaddr_in address;
	int address_len;
	char *buffer;
} request;

/*builds log output for the server when requests are processed*/
void make_log(char *str, int code){
	
	struct sockaddr_in thisAddr = request.address;
	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	//start building the log output
	strcpy(log_string, asctime (timeinfo));
	//remove "\n" from buffer string
	int i;
	for(i = 0; i < strlen(log_string); i++){
		if (log_string[i] == '\n') log_string[i] = ' ';
	}
	if(str[strlen(str)-1] == '/') strcat(str, "index.html");
	printf("%s %s %d %s %d ", log_string,inet_ntoa(request.address.sin_addr),request.client_socket,str,code);
	if (code == 400){
		printf(": bad request\n");
		sendto(request.client_socket,"400: bad request, GET commands only\n", 36, 0, (struct sockaddr *)&request.address, request.address_len);
	}
	else if (code == 404){
		char *response = "404: not found\n";
		printf("%s\n", response);
		sendto(request.client_socket,response, strlen(response), 0, (struct sockaddr *)&request.address, request.address_len);
	}
	else if (code == 200)printf("ok\n");
}

//here the work of determing if the requested file exist, and packaging it up for the client gets done
int getFile(char *path){
	FILE* file;  //requested file
	char *contents;//gets filled with file contents for packaging
	unsigned long int size = 0;
	//build the full path from cwd
	char *full_path = strcat(cwd, path);
	//printf("%s\n", full_path);  //just checking where I got to
	//printf("%s\n", path);
	if( access( full_path, F_OK ) < 0 ) {	//make sure that a file is there
		code = 404; //couldn't get file, so 404
		getcwd(cwd, 1024);  //reset cwd for next request.
		return 1;
	}
	else{
		code = 200; //client request is good, so let's fullfill it
		file = fopen(full_path, "r");//start reading the file
		/*find the size of the file to be sent */
		fseek(file, 0, SEEK_END);    //find beginning and end of file
		size = ftell(file);  //need to know the size of packet to send back
		//end of size finding*
		rewind(file);  //not sure if needed
		contents = calloc(size+1,1);//make packet contents the size it should be

		fread(contents,1, size, file);
		//printf("%s\n", contents);
		
	//start building the packet to send back
	char *fullfill = "200: ok;";
	char *packet;
	packet = malloc(strlen(fullfill) + strlen(contents));  //initialize packet size
	//strcat(packet, "http/1.0 ");
	strcat(packet, fullfill);
	strcat(packet, "\n");
	
	if (strlen(contents) < 500){//packet built, and can be sent all at once
		strcat(packet, contents);
		sendto(request.client_socket, packet, strlen(packet), 0, (struct sockaddr *)&request.address, request.address_len);
	}
	else{//if the file is big, we'll have to send it over several packets
		strcat(packet, "\n");
		strncat(packet, contents, 500);		
		sendto(request.client_socket, packet, strlen(packet), 0, (struct sockaddr *)&request.address, request.address_len);
		int i = 500;
		while(i < size){
			strncpy(packet, &contents[i], 500);
			sendto(request.client_socket, packet, strlen(packet), 0, (struct sockaddr *)&request.address, request.address_len);
			i +=500;
		}
		sendto(request.client_socket, "\n",1, 0, (struct sockaddr *)&request.address, request.address_len);
	}
	
	//clean up
	//printf("%s\n", packet);
	free(packet);
	free(contents);
	getcwd(cwd, 1024);  //reset cwd for next request.
	return 2; 
	}
	return -1;
}
/*if we've received a request, we'll parse the string here and handle appropriately 
*/ 
void handleRequest(){

	char path[1024];  //well parse the request string for this
	bzero(path, 1024); //if you don't initialize this business, you get garbage
	//printf("got to handleRequest() with the message\n%s\n",str);  //REMOVE THIS LINE FROM finished product
	//determine the time that the request was received
//400	
	//since sws only responds to GET requests, make sure that's what we have	
	if(strncmp("get ",request.buffer,3) != 0 & strncmp("GET ", request.buffer,3) !=0){
		//make_log(request.buffer,400);//code = 400;  //bad request
		code = 400;
	}
	else{
//404, 200. correct request syntax, so let's look for the file
	//find the start of the file path. Strip off the "GET "
	int start = 4;
	int end = 5;
	//go to the end of the request string
	while(request.buffer[end] != ' ' & request.buffer[end] != '\0'){
		end++;
	}//should have start and stop boundaries for the file requested
	strncpy(path, &request.buffer[start], end-start);//path parsed!
	//printf("%s\n", path); //error checking
	//like a grownup server, assume looking for index if no file specified at end of path
	if(path[strlen(path)-1] == '/')strcat(path, "index.html");
	//check and see if naughtiness is at hand
	if(strncmp("/..", path, 2) == 0){
		sendto(request.client_socket,"400: bad request, you may not go backwards in directory\n", 57, 0, (struct sockaddr *)&request.address, request.address_len);
		code = 400;
	}
	else{
	//if we get here, check and see if the request points to anything (determine 200 or 404)
	printf("%s\n",path);
	int reqcess = getFile(path);  //"reqcess" is portmanteau of "request success(?)", which is pretty cute
	//returns 1 if 404 error, 2 if file found, and -1 as a default (which should never get returned)
	if (reqcess == -1) fprintf(stderr,"something went wrong while looking for a requested file\n");
	}
	}
	make_log(request.buffer, code);
	bzero(request.buffer, strlen(request.buffer));
	bzero(path, strlen(path));
	code = -1;
}

//if some subdirectory was declared, add it to the end of cwd
void parse_directory(char string[]){
//should we insert a '/' to preserve directory structure?
		if(string[0] != '/')strcat(cwd, "/"); 
		strcat(cwd, string);
		printf("the current working directory is %s\n",cwd);
}

//most of the following code was supplied as part of lab instruction. I made it pass requests to other functions
//in order to meet assignment criteria
int checkReadyForRead(int sockfd)
{
    while (1){
        char read_buffer[MAXBUFLEN];
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);
        int retval = select(sockfd+1, &readfds, NULL, NULL, NULL);
        if(retval <=0) //error or timeout
            return retval;
        else
        {
            if(FD_ISSET(STDIN_FILENO, &readfds) &&
               (fgets(read_buffer, MAXBUFLEN, stdin) != NULL) &&
               strchr(read_buffer, 'q') != NULL)  // 'q' pressed, exit gracefully. This means letting all server requests finish up

                return 1;
            else if(FD_ISSET(sockfd, &readfds))   // recv buffer ready
                return 2;
        }
		
    }
    return -1;
}

int main(int argc, char *argv[]){
    //establish what is the current working directory (cwd)
	getcwd(cwd, 1024);
	
    //socket/port initialization code taken from lab 
    //domain is PF_INET, type is SOCK_DGRAM for UDP, and protocol can be set to 0 to choose the proper protocol!
    int sockfd, portno;
    socklen_t cli_len;
    char buffer[MAXBUFLEN];	//data buffer
    struct sockaddr_in serv_addr, cli_addr;	//we need these structures to store packet info
    int numbytes;
    
    if (argc < 2) {
        printf( "Usage: %s <port>\n", argv[0] );
        fprintf(stderr,"ERROR, no port provided\n");
        return -1;;
    }
   	if(argc < 3){ //probably should demand that some sub dir is supplied, but I'm an easy going sort
		printf("no subdirectory was specified. Using this directory\n");	
	}
	else if(argv[2] != NULL){
	//make sure it's not trying to go down a place in directory tree height, not a big security concern as 
	//I start the server, but a similar idea is used when a client tries to fetch something, so why not?
		if(strncmp(argv[2], "/..", 3) == 0 || strncmp(argv[2], "..",2) ==0){
			fprintf(stderr,"ERROR, you may not go back a directory for file serving\n");
       		return -1;
		}
	 		parse_directory(argv[2]);

//make sure that this is actually a directory	
    	if (chdir(cwd) != 0){
		fprintf(stderr, "ERROR, That is not a directory\n");
		return -1;
		}
	}

    //The first step: creating a socket of type of UDP
    //error checking for every function call is necessary!
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("sws: error on socket()"); 
        return -1;			
    }
//again, I should point out that where UDP concerns are initialized is not my original work,
//but why reinvent the wheel?
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);  //read the port number value from stdin
    serv_addr.sin_family = AF_INET;	//Address family, for us always: AF_INET
    //serv_addr.sin_addr.s_addr = inet_addr("142.104.69.255");
    serv_addr.sin_addr.s_addr = inet_addr("10.10.1.100"); //INADDR_ANY;  //Listen on any ip address I have
    serv_addr.sin_port = htons(portno);  //byte order again
    
    //Bind the socket with the address information:
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0){
        close(sockfd);
        perror("sws: error on binding!");
        return -1;
    }
    
    while(1){
        cli_len = sizeof(cli_addr);
        int ret = checkReadyForRead(sockfd);  //if I was building this from the ground up, I'd have used
        if(ret == 1){			//threading and handled concurrency a bit more sleekly. Next assignment
	close(sockfd);				
            exit(1);  //one returned when 'q' is pressed
	}
        //the sender address information goes to cli_addr
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN-1 , 0,
                                 (struct sockaddr *)&cli_addr, &cli_len)) == -1) {
            perror("sws: error on recvfrom()!");
            return -1;
        }//end of server setup
        buffer[numbytes] = '\0';
        if(ret == 2){
		//fill out the global request struct. another idea that I hope to improve on the next assignment
			request.client_socket = sockfd;  //which I'm guessing will be a TCP-based server
			request.address = cli_addr;
			request.address_len = cli_len;
			request.buffer = buffer;
			handleRequest();  
			bzero(buffer, strlen(buffer));
		}
        if ((numbytes = sendto(sockfd, buffer, strlen(buffer), 0,
                               (struct sockaddr *)&cli_addr, cli_len)) == -1) {
            perror("sws: error in sendto()");
            return -1;
        }
    }
    
    close(sockfd);

    return 0;
}
