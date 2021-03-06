#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rpc/xdr.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define BUFLEN 128
#define MAX_CLIENT 2

#define MSG_OK "+OK\n"
#define MSG_ERR "-ERR\n"

int prot_a (SOCKET);
int prot_x (SOCKET);
int readline (SOCKET, char *, int);
int writen(SOCKET, char *, int);

int childCounter = 0;

void handler(int sig)
{
	int status;
    	while (waitpid(-1, &status, WNOHANG)>0){
		childCounter--;
		printf("Decreased the counter! Actual value: %d\n", 			childCounter);	
   	};
}

int main (int argc, char *argv[]) {
	
	SOCKET s, sa;
	int err;
	/*Server stuff*/
	uint16_t port_h, port_n;
	struct sockaddr_in servaddr;
	/*Clients stuff*/
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);
	
	//Signal SIGCHILD management
	sigset_t mask;
	sigset_t orig_mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGCHLD);
	signal(SIGCHLD, handler);
	
	if(argc!=3){
		printf("Error with inputs\n");
		exit(1);
	}
	
	if ((s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0){
		printf ("Error creating the socket\n");
		exit(1);
	}
	
	//necessario? non posso fare atoi(argv[2]) semplicemente?
	port_h = atoi(argv[2]);
	port_n = htons(port_h);
	
	/* create the socket */
    	printf("Creating socket\n");
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		printf("socket() failed");
	printf("done. Socket fd number: %d\n",s);
	
	/* specifiy server address structure */
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = port_n;
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
	
	if (bind(s, (struct sockaddr_in *)&servaddr, sizeof(servaddr)) != 0 ) {
		printf("Error in bond()\n");
		exit(1);
	}
	
	printf("READY: socket %d binded to %s:%u\n", s, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
	
	/* Activate socket listening */
	listen(s,MAX_CLIENT);
	
	while(1){
		printf("Main loop entered. Accepting connections...\n");
		
		/* temporarily mask/block SIGCHLD signals and save original mask */
		if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
		    printf ("sigprocmask - ERROR\n");
		}

		if(childCounter>=MAX_CLIENT) {
			printf("too many connections, not accepting anymore...!\n");
			wait();
			childCounter--;
			printf("I am accepting again!\n");
		}
		/* restore original mask */ 
		if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
		    printf ("sigprocmask - ERROR\n");
		}
		
		/*
		//Disabling the SIGCHILD now I have to check again
		int status2;
    		while (waitpid(-1, &status2, WNOHANG)>0){
			childCounter--;
			printf("Decreased the counter! Actual value: %d\n", childCounter);	
   		};
   		*/
		
		printf("Now accepting with counter: %d\n", childCounter);	
		sa = accept(s, (struct sockaddr_in *)&cliaddr, &cliaddrlen);
		printf("Socket active: %d. Connection from client %s:%u\n", sa, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		
		if(fork()==0){
			//child code
			if ( close(s) != 0 ) {
				printf("Problems trying close(s) in child\n");			
				exit(1);
			}
			switch (argv[1][1])
			{
				case 'a':
					err = prot_a(sa);
					break;
				case 'x':
					err = prot_x(sa);
					break;
			}
			if ( close(sa) != 0 ) {
				printf("Problems in disconnecting from client\n");			
				exit(1);
			}
			printf("Connection closed\n");
			exit (0);
		} else {
			//parent code
			if ( close(sa) != 0 ) {
				printf("Problems trying close (sa) in parent\n");			
				exit(1);
			}
			childCounter++;
			printf("Actual counter: %d\n", childCounter);
		}
	}
	return 0;
}
	
int prot_a (SOCKET sa) {
	
	int nread, fsize;
	char buffer[BUFLEN], buffertmp[BUFLEN], *filename;
	
	while (1) {
		printf("Waiting for the file name\n");
		nread = readline(sa,buffer,BUFLEN);
		printf("Received line: %s", buffer);
		if ( nread == 0 ) {
			printf("Client disconnected\n");
			break;
		}
		
		if ( nread == -1 ) {
			printf("Reception problems...\n");
			break;
		}
		
		strncpy(buffertmp,buffer,3);
		buffertmp[3]='\0';
		
		if(!strcmp(buffertmp, "GET")) {
			printf("GET received!\n");
			filename = strtok(buffer,"GET\n");
			printf("Il nome del file: %s\n", filename);
  			
  			//send the file to the client......
			char fs_name[BUFLEN] = "/home/parallels/Desktop/";
			if(filename==NULL){
				printf("-ERR\n");
				writen(sa, MSG_ERR, strlen(MSG_ERR));
				printf("No file name was specified!\n");
				continue;
			}
			strcat(fs_name,filename);
			printf("File path: %s\n",fs_name);
			FILE *fr = fopen(fs_name, "r");
			if(fr==NULL){
				printf("-ERR\n");
				writen(sa, MSG_ERR, strlen(MSG_ERR));
				printf("File name wrong/Error opening the file\n");
			} else {
				char sdbuf[BUFLEN];
				char tmp[BUFLEN];
				//reading file size
				fseek(fr, 0L, SEEK_END);
				fsize = ftell(fr);
				fseek(fr, 0L, SEEK_SET);
				//sending info and synch tokens
  				sprintf (tmp, "%d", fsize);
				strcat (tmp,"\n");
				writen(sa, MSG_OK, strlen(MSG_OK));
				writen(sa, tmp, strlen(tmp));
				
				bzero(sdbuf, BUFLEN); 
		    		int fs_block_sz; 
		    		int t = 0;
		    		printf("Sending..........\n");
		    		while((fs_block_sz = fread(sdbuf, sizeof(char), BUFLEN, fr))>0)
		    		{
		      			if(writen(sa, sdbuf, fs_block_sz*sizeof(char)) < 0)
		        		{
		          			fprintf(stderr, "ERROR: Failed to send file %s.\n", fs_name, errno);
		            			return 1;
		        		}
		        		//printf("Send part %d\n",t);
		       			bzero(sdbuf, BUFLEN);
		       			t++;
		    		}
		    		printf("Sent to client!\n");
			}
			continue;
		} else {
			strncpy(buffertmp,buffer,4);
			buffertmp[4]='\0';
			if(!strcmp(buffertmp, "QUIT")){
				printf("QUIT received, closing connection...\n");
				break; //exiting 
			}
		} 
		//if here the incoming string was not meaningful
		printf("-ERR\n");
		char tmp2[BUFLEN] = MSG_ERR;
		writen(sa, tmp2, strlen(tmp2));
	}
	
	return 0;
}

int prot_x (SOCKET sa) {return 0;}

int readline (SOCKET s, char *ptr, int maxlen)
{
    int n;
    int nread;
    char c;

    for (n=1; n<maxlen; n++)
    {
	nread=recv(s, &c, 1, 0);
	if (nread == 1)
	{
	    *ptr++ = c;
	    if (c == '\n') 
		break;
	}
	else if (nread == 0)	/* connection closed by party */
	{
	    *ptr = 0;
	    return (n-1);
	}
	else 			/* error */
	    return (-1);
    }
    *ptr = 0;
    return (n);
}
	
	
int writen(SOCKET s, char *ptr, int nbytes)
{
    int  nleft, nwritten;

    for (nleft=nbytes; nleft > 0; )
    {
	nwritten = send(s, ptr, nleft, 0);
	if (nwritten <=0)
	    return (nwritten);
	else
	{
	    nleft -= nwritten;
	    ptr += nwritten;   
	}
    }
    return (nbytes - nleft);
}	
	
