#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <limits.h>

#define CHILDNUM 2
#define BUF_SIZE 258

char rec_msg[BUF_SIZE] = {0};
pid_t children[CHILDNUM] = {0};
int sock;
int pipefd[2];
int pipefd2[2];

/*
 *  The function printMenu() will print out the 
 *  different choices a user has when using the 
 *  program. The function is called from 
 *  commandloop(). 
 * 
 *  Return: void.
 */
void printMenu() {
    printf("1. Get one job from server\n");
    printf("2. Get X number of jobs from server\n");
    printf("3. Get all jobs from server\n");
    printf("4. Quit\n");

    return;
}
/*
 * The function connect_to_server() takes two arguments,
 * which are the same as the main function. I first did 
 * the same thing as in plenumstime uke 10, but to 
 * implement the bonus task I changed to getaddrinfo.
 * I based my code on the example from getaddrinfos 
 * man-page. Addrinfo is a struct with different values
 * that specify different criteria for selecting the 
 * socket address structures. hints points to one such
 * struct. I set socktype to be sockstream, because that's
 * what was used during plenumstime uke 10. The function will loop 
 * through possible IP's and if connect succeeds, it 
 * will stop.
 * Return: returns 0 on success, and -1 on failure.
 */
int connect_to_server(int argc, char *argv[]) {

    int port_max_value = USHRT_MAX;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    
    if(argc != 3) {
	fprintf(stderr, "Usage: %s <IP/Host> <port>\n", argv[0]);
	return -1;
    }
    int port_num = atoi(argv[2]);
    if(port_num > port_max_value) {
	printf("Port number is too high.\n");
	return -1;
    }
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    
    if((s = getaddrinfo(argv[1], argv[2], &hints, &result)) != 0) {
	perror("getaddrinfo()");
	return -1;
    }
    for(rp = result; rp != NULL; rp = rp->ai_next) {
	sock = socket(rp->ai_family, rp->ai_socktype,
		      rp->ai_protocol);
	if(sock == -1) {
	    continue;
	}
	if(connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
	    break;
	}
	close(sock);
    }
    if(rp == NULL) {
	fprintf(stderr, "Could not connect.\n");
	freeaddrinfo(result);
	return -1;
    }
    freeaddrinfo(result);

    printf("Connected\n");
    return 0;
}

/*
 * The function recieve() will recieve messages from the server.
 * The recieved message is a char[] where [0] is the type, [1] is
 * the length and the rest is the message itself. If the type is
 * something other than 'E', 'O' or 'Q', the function will 
 * fail. Learned how to use recv() from plenumstime uke 10
 * Return: returns 0 on success and -1 on failure.

 */
int recieve() {

    ssize_t ret = recv(sock, rec_msg, sizeof(rec_msg) -1, 0);
    if(ret == -1) {
	perror("recv()");
	close(sock);
	return -1;
    }
    if(rec_msg[0] != 'E' && rec_msg[0] != 'O' && rec_msg[0] != 'Q' ) {
	printf("Recieved %c, which is not a good job_type.\n", rec_msg[0]);
	return -1;
    }

    printf("recieved type is: %c\n", rec_msg[0]);

    if(rec_msg[0] == 'Q') {
	printf("No more jobs.\n");
	return 0;
    }
    return 0;
}

/*
 * The function send_msg will send a message to the client.
 * It takes one argument, char msg, which is the message that
 * should be sent. Learned how to use send() from 
 * plenumstime uke 10.
 * Return: returns 0 on success and -1 on failure.
 */

int send_msg(char msg) {
    ssize_t ret = send(sock, &msg, sizeof(char), 0);
    if(ret == -1) {
	perror("send()");
	close(sock);
	return -1;
    }
    return 0;
}
/*
 * The function send_to_child() takes two 
 * arguments, pipefd and pipefd2. Both 
 * are pointers to the pipes that write to
 * the child processes. The function is
 * called on from the commandloop
 * function. The function calls
 * on write to a one or both of the pipes
 * depending on what type of message the
 * client has recieved. 
 *
 * Return: returns 0 on success, -1 on failure.
 * When the job is done and 'Q' is sent to the
 * children, the function will return 1 instead 
 * of 0, this is so that the commandloop will know
 * when to end.
 */

int send_to_child() {

    unsigned char len = rec_msg[1];
    rec_msg[len+2] = '\0';
    if(rec_msg[0] == 'O') {
	printf("Sending to child 1.\n");
	if(write(pipefd[1], rec_msg, len+2) == -1) {
	    perror("write()");
	    return -1;
	}
    }
    else if(rec_msg[0] == 'E') {
	printf("Sending to child 2.\n");
	if(write(pipefd2[1], rec_msg, len+2) == -1) {
	    perror("write()");
	    return -1;
	}
    }
    else {
	if(write(pipefd[1], rec_msg, 1) == -1) {
	    perror("write()");
	    return -1;
	}	
	if(write(pipefd2[1], rec_msg, 1) == -1) {
	    perror("write()");
	    return -1;
	}
	return 1;
    }
    return 0;
}

/*
 * The function commandloop() will accept input from a user
 * with scanf() and do different things based on that input. 
 * The choices are presented in the function printMenu(). 
 * The function takes two arguments, both are pointers to pipes. 
 * The arguments are used when calling on the function 
 * send_to_child. The loop also calls on the 
 * functions recieve(), send_msg(), atoi() and sleep().
 * The argument used in send_msg can be 'G', 'T' or 'E',
 * depending on the situation. When the client wants
 * a message from the server, a 'G' is sent. If the
 * client is done and terminates normally, a 'T' is sent.
 * If something goes wrong, an 'E' is sent.
 * atoi is used to make an input from the user into an
 * integer when the user wants to get X jobs.
 * Just to make the print a little easier to read, I 
 * call usleep() for 100000 microseconds before printing the menu.
 * I also call usleep() for 50000 microseconds 
 * when printing X-number of jobs or all jobs,
 * to make the printing look prettier. If I don't,
 * the print is instantaneous, which makes it harder to check if
 * everything was printed. 
 * Because the loop is waiting for an input before doing 
 * anything, if the server shuts down, the client will still
 * run until a user inputs something. 
 *
 * Return: On success the function returns 0, on
 * failure it returns -1. 
 */

int commandloop() {
    while(1) {
	char input;
	usleep(100000);
	printMenu();
	printf("Write a number:\n");
	scanf("%s", &input);
	while(input != '4') {
	    printf("input is: %c\n", input);
	    if(input == '1') {	    
		send_msg('G');
		if(recieve() == -1) {		   
		    send_msg('E');
		    return -1;
		}
		if(send_to_child() == 1) {		   
		    send_msg('T');
		    return 0;
		}
		break;
		input = '0';
	    }
	    if(input == '2') {
		printf("Write a number for how many jobs you want:\n");
		char num = '0';
		
		scanf("%s", &num);
		
		if((num >= '!' && num <= '/') ||(num >= 'a' && num <= 'z') ||
		   (num >= 'A' && num <= 'Z')) {
		    printf("That's not a number...\n");
		}
		else {
		    int num2 = 0;
		    num2 = atoi(&num);			
		    printf("Getting: %d jobs.\n", num2);
		    for(int i = 0; i < num2; i++) {
			usleep(50000);
			send_msg('G');
			if(recieve() == -1) {			
			    send_msg('E');
			    return -1;				
			}
			if(send_to_child() == 1) {
			    send_msg('T');
			    return 0;
			}
		    }		
		}
		input = '0';
	    }
	    if(input == '3') {
		printf("Getting all jobs.\n");
	
		while(1) {
		    usleep(50000);
		    send_msg('G');
		    if(recieve() == -1) {			
			send_msg('E');
			return -1;				
		    }
		    if(send_to_child() == 1) {
			send_msg('T');
		        return 0;
		    }
		}
		
	    }
	    else if(input == '0') {
		
		printMenu();
		printf("Write a number:\n");
		scanf("%s", &input);	    
	    }
	    else {
		printf("Not a valid input.\n");
		input = '0';
	    }
	}
	if(input == '4') {
	    send_msg('T');
	    printf("Exiting program.\n");
	    for(int i = 0; i<CHILDNUM; i++) {
		kill(children[i], SIGTERM);
	    }
	    return 0;
	}
    }	    		    
    return 0;
}


/*
 * The function sig() takes no arguments.
 * It is called from the signal() function 
 * in main. It sends an 'E' to the server 
 * so that the server can know that the 
 * client terminates abnormally.
 * (I'm a bit uncertain if ctrl + c is 
 * a normal way to stop a program or not)
 * It then ends the child processes. 
 * When it's done it exits.
 *
 * Return: void.
 */
void sig() {
    printf("\n Got a ctrl + c! \n");
    send_msg('E');
    for(int i = 0; i< CHILDNUM; i++) {	
	if(kill(children[i], SIGTERM) == 0) {
	    printf("killing: %d\n", children[i]);
	}
    }
    exit(EXIT_SUCCESS);
}



/*
 * The main function takes two arguments, 
 * int argc and char *argv[]. Both are 
 * used in the connect_to_server() function.
 * main() will open the two pipes with pipe()
 * and fork twice with fork(). 
 * The child processes will wait until they recieve
 * something through their pipe, and print
 * what they get. After the piping and 
 * forking is done, the parent process should 
 * call on the  connect_to_server() function and the 
 * commandloop() function. The children recieve the 
 * whole message through the pipe, with length and
 * type. This is so that a message can start with 
 * 'Q' without the children terminating.
 * 
 * Return: returns 0 on success, exits on failure.
 */

int main(int argc, char *argv[]) {
    int parent = 0;
    int id = 0;
  
    if(pipe(pipefd) == -1 ) {
	perror("pipe()");
	exit(EXIT_FAILURE);
    }
    if(pipe(pipefd2) == -1) {
	perror("pipe()");
	exit(EXIT_FAILURE);
    }
  
    for (int i = 0; i < CHILDNUM; i++) {
	id++;
	parent = fork();	
	if(parent == -1) {
	    perror("fork()");
	    exit(EXIT_FAILURE);
	}
	if(!parent) { //child	   	    
	    break;
	}
	children[i] = parent;
    }
    if(!parent) {
	while(1) {
	    char buf[BUF_SIZE] = {0};
	    char buf2[BUF_SIZE] = {0};
	   
	    if(id == 1){		
		if(read(pipefd[0], buf, sizeof(buf)) == -1) {		   
		    exit(EXIT_FAILURE);
		}
		if(buf[0] == 'Q') {
		    printf("Child %d is terminating.\n", id);		  
		    exit(EXIT_SUCCESS);
		}		
		fprintf(stdout,"%s\n", buf+2);	
	    }
	    else if(id == 2) {		
		if(read(pipefd2[0], buf2, sizeof(buf2)) == -1) {
		    exit(EXIT_FAILURE);
		}
		if(buf2[0] =='Q') {
		    printf("Child %d is terminating.\n", id);		   
		    exit(EXIT_SUCCESS);
		}		
		fprintf(stderr, "%s\n", buf2+2);	
	    }
	    else {
		printf("Child %d doesn't have anything to do!\n", id);
		exit(EXIT_SUCCESS);
	    }
	}
    }   
    
    if(parent) {
	signal(SIGINT, sig);

	if(connect_to_server(argc, argv) != 0) {
	    perror("connect_to_server()");
	    for(int i = 0; i < CHILDNUM; i++) {
		printf("killing: %d\n", children[i]);
		kill(children[i], SIGTERM);
	    }
	    exit(EXIT_FAILURE);
	    
	}
	if(commandloop() == -1) {
	    for(int i = 0; i < CHILDNUM; i++) {
		if(kill(children[i], SIGKILL)) {
		    printf("killing: %d\n", children[i]);
		}
	    }
	    exit(EXIT_FAILURE);
	}
    }
    return 0;
}   
