#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <netdb.h>
#include <signal.h>
#include <limits.h>

#define BUF_SIZE 256
//Need these variables several places
int sock;
int client_sock;
int file_closed = 0;
int file_open = 0;
int con_sock = 0;
FILE *file;
/*
 * The function create_socket()takes a char
 * pointer as argument. It points at the  
 * port. The function is pretty similar to
 * the one used in the client. I also got
 * this function from the man-pages. One difference
 * is the setsockopt() function which is called
 * to make sure that the socket can be used 
 * with the same ip and port again immidiately 
 * after it's been used. I got setsockopt and bind
 * from plenumstime uke 10. Another difference is 
 * the bind() function which is called to bind 
 * a socket to an ip. After this is successful
 * the function will call the listen function
 * which listens for connections from a client.
 * Those connections are then accepted in the
 * accept_connections() function.
 * 
 * Return: returns the socket on success,
 * so that it can be used in accept_connections()
 * and -1 on failure.
 */
int create_socket(char *port) {
    int port_max_value = USHRT_MAX;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
  
    int s;

    int port_num = atoi(port);
    if(port_num > port_max_value) {
	printf("Port number is too high.\n");
	return -1;
    }
    if(port_num < 1024) {
	printf("Port number is too low.\n");
	return -1;
    }
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, port, &hints, &result);
    if(s != 0) {	
	perror("getaddrinfo()");
	return -1;
    }
    int yes = 1;
  
    for(rp = result; rp != NULL; rp = rp->ai_next) {

	sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	if( sock == -1) {
	    continue;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
	    perror("setsockopt()");
	    close(sock);
	    return -1;
	}
	if(bind(sock, rp->ai_addr, rp->ai_addrlen) == 0) {	    
	    printf("Bound successfully\n");
	    break;
	}
	close(sock);
    }
    if(rp == NULL) {
	perror("bind()");
	return -1;
    }    
    freeaddrinfo(result);

    if(listen(sock, SOMAXCONN)) {
	perror("listen()");
	close(sock);
	return -1;
    }
    return sock;
    
}

/*
 * The function accept_connections() takes a socket 
 * as an argument. It uses the struct sockaddr_in
 * to get the client's address and uses the 
 * accept() function to accept connections.
 * I got this function from plenumstime uke 10.
 * 
 * Return: returns 0 on success and in errno == EINTR,
 * returns -1 on failure.
 */
int accept_connections(int sock) {
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = sizeof(client_addr);
    client_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
    if(con_sock > 0) {
	printf("Refusing connection.\n");
    }
    else {
	if(client_sock == -1) {
	    /* EINTR = system call was interrupted by a signal that was 
	       caught before a valid connection arrived */
	    if(errno == EINTR) {
		return 0;
	    }
	    perror("accept()");
	    close(sock);
	    return -1;
	}
	
	printf("Client connected!\n");
	con_sock ++;

    }
    return 0;
}

/*
 * The function read_file() takes one argument,
 * a char pointer to a filename. The function 
 * will not only read a file, it will also
 * send to and recieve messages from
 * the client. The function will first open 
 * the file for reading and then get 
 * the file_length from the stat struct.
 * 
 * I got the idea to use stat from 
 * stackoverflow after searching for a way 
 * to get the length of a file. After that 
 * i also found out that fread would have
 * returned -1 when it reaches
 * the end of the file, and using that could
 * maybe have been easier. 
 * 
 * After getting the length of the file the 
 * function will enter a loop and continue 
 * to be in that loop until it recieves 
 * a 'T' or an 'E' from the client. 
 * If the server recieves an 'T' or 'E'
 * while there still are more jobs left,
 * it will accept a new connection. 
 * In the loop the function will read and
 * send the file to the client, unless there
 * is something wrong with the job_type that is
 * read. When the function has read the entire
 * file, it will send a 'Q' to the client and 
 * end. The way I use send() and recv() is 
 * from plenumstime uke10. If the client sends
 * something other than 'T', 'E' or 'Q', the 
 * server will send a 'W' back just to be mean.
 *
 * Return: returns 0 on success and -1 on failure.

 */
int read_file(char* filename) {
    file = fopen(filename, "r");
    if(file == NULL) {
	fprintf(stderr, "Could not open file: %s\n", filename);
	return -1;
    }
    file_open = 1;
    int count = 0;
    int file_length = 0;
    struct stat st;
    if(stat(filename, &st) == 0) {
	file_length = st.st_size;
    }
    else {
	perror("stat()");
	return -1;
    }
    char job_type = 0;
    unsigned char job_length = 0;
    char job_text[BUF_SIZE] = {0};
    char to_send[BUF_SIZE + 2] = {0};
    char buf = 0;
    
    while(1) {	
	ssize_t ret = recv(client_sock, &buf, 1, 0);
	if(ret == -1) {
	    if(errno == EINTR) {
		return 0;
	    }
	    perror("recv()");
	    return -1;
	}
	if(buf != 'T' && buf != 'E' && buf != 'G') {
	    printf("Recieved something unexpected: '%c'.\n", buf);
	    printf("Sending something unexpected back.\n");
	    char msg = 'W';
	    ssize_t ret = send(client_sock, &msg, sizeof(char), 0);	   
	    if(ret == -1) {
		perror("send()");
		close(client_sock);
		con_sock = 0;
		return 0;
	    }
	    if(file_closed == 0) {
		fclose(file);
		file_closed = 1;
	    }
	    return -1;
	}
	
	if(buf == 'T') {
	    printf("Recieved 'T'. Client terminating normally.\n");
	    close(client_sock);
	    con_sock = 0;
	    printf("Waiting for new connection.\n");
	    if(accept_connections(sock) == -1) {
		perror("accept_connections()");
		close(sock);
	        return -1;
	    }
	}    	
	if(buf == 'E') {
	    printf("Recieved 'E'. Client terminating abnormally.\n");	  
	    close(client_sock);
	    con_sock = 0;
	    printf("Waiting for new connection.\n");
	     if(accept_connections(sock) == -1) {
		perror("accept_connections()");
		close(sock);
	        return -1;
	    }	   
	}
	if(count == file_length) {
	    if(file_closed == 0) {
		fclose(file);
		file_closed = 1;
	    }
	    char msg = 'Q';
	    ssize_t ret = send(client_sock, &msg, sizeof(char), 0);	   
	    if(ret == -1) {
		perror("send()");
		close(client_sock);
		con_sock = 0;
		return 0;
	    }
	    ret = recv(client_sock, &buf, 1, 0);
	    if(buf == 'T') {
		printf("Client terminating normally.\n");
	    }
	    else {
		printf("Client terminating abnormally.\n");
	    }
	    printf("No more jobs, server shutting down.\n");

	    return 0;
	}
	
	else if(count < file_length && buf != 'T') {	    
	    fread(&job_type, sizeof(char), 1, file);
	    fread(&job_length, sizeof(unsigned char), 1, file);
	    if(job_length > file_length) {
		printf("The length is wrong, sending 'Q' instead.\n");
		char msg = 'Q';
		ssize_t ret = send(client_sock, &msg, sizeof(char), 0);
		if(ret == -1) {
		    perror("send()");
		    close(client_sock);
		    con_sock = 0;
		    return 0;
		}
		if(file_closed == 0) {
		    fclose(file);
		    file_closed = 1;
		}
		close(client_sock);
		return 0;
	    }    
	    
	    fread(&job_text, job_length, 1, file);
	    
	    count += job_length + 2;
	   	    
	    to_send[0] = job_type;
	    if(job_type != 'E' && job_type != 'O' && job_type != 'Q') {
		ssize_t ret = send(client_sock, &to_send, 1, 0);
		if(ret == -1){		    
		    perror("send()");
		    close(client_sock);
		    con_sock = 0;
		    return -1;
		}
		    
		ret = recv(client_sock, &buf, 1, 0);
		if(ret == 0) {
		    printf("Client has disconnected.\n");
		}
		else if(ret == -1) {
		    if(errno == EINTR) {
			return 0;
		    }
		    perror("recv()");
		    
		    return -1;
		}
	    }
	    else{
		to_send[1] = job_length;
		strcpy(to_send+2, job_text);
		if(buf == 'G') {
		    printf("Sending job!\n");		
		    ssize_t send_job = send(client_sock, &to_send,
					    job_length + 2, 0);
		    if(send_job == -1) {
			perror("send()");
			close(client_sock);
			con_sock = 0;
			return -1;
		    }
		}
	    }
	}
    }
    return 0;
}

/* 
 * The function sig() takes no arguments.
 * It is called in the signal() function 
 * from the main function. sig() sends 
 * a 'Q' to the client so that it can
 * shut down, and closes sockets and the
 * file. If the client already has shut down
 * it will get an error on send, but 
 * closes the rest of the stuff anyway.
 * When it's done it exits the program.
 * 
 * Return: void. 
 */
void sig() {
    printf("\n Got a ctrl + c! \n");
    char msg = 'Q';
    ssize_t ret = send(client_sock, &msg, sizeof(char), 0);
    if(ret == -1) {
	perror("send()");
    }
    if(file_closed == 0 && file_open == 1) {
	fclose(file);
	file_closed = 1;
    } 
    close(sock);
    close(client_sock);
    exit(EXIT_SUCCESS);
}

/* The main function takes two arguments,
 * argc and argv[]. If argc is something else
 * than 3, the program will exit, because the
 * three arguments are supposed to be the program
 * itself, a filename and a port. The port is used
 * in the create_socket() function and the filename
 * is used in the read_file() function. The only thing
 * the main method does is call on signal() and the other 
 * functions in the program and exiting when that's done.
 * 
 * Return: returns 0 on success and 
 * just exits on failure.
 */

int main(int argc, char *argv[]) {
    
    if(argc != 3) {
	fprintf(stderr, "Usage: %s <FILENAME> <port>\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    signal(SIGINT, sig);

    sock = create_socket(argv[2]);
    if(sock == -1) {
	perror("create_socket()");
	close(sock);
	exit(EXIT_FAILURE);
    }

    if(accept_connections(sock) == -1) {
	perror("accept_connections()");
	close(sock);
	exit(EXIT_FAILURE);
    }
    
    if(read_file(argv[1]) == -1) {
	perror("read_file()");
	close(sock);
	exit(EXIT_FAILURE);
    }
   
    close(sock);
    return 0;
}
