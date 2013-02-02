#include "Server.h"
#include "ServerFunc.h"
#include "ServerMacro.h"


int main(int argc,char **argv)
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int server_sockfd, client_sockfd;
    socklen_t server_len, client_len;
    int result;
    fd_set readfds, testfds;

	//  Create and name a socket for the server.

    if(FALSE == (server_sockfd = socket(AF_INET,SOCK_STREAM,0)))
    {
        // Create socket failed, report error
        errorreport(GET_SOCKET_ERR);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    server_len = sizeof(server_addr);

	// Socket bind

    if(FALSE == (bind(server_sockfd,(struct sockaddr *)&server_addr ,sizeof(server_addr)))) 
    {
        // Socket bind failed, report error
        errorreport(SOCKET_BIND_ERR);
        exit(1);
    }

	// Create a connection queue and initialize readfds to handle input from server_sockfd.

    if(FALSE == listen(server_sockfd, MAX_CONNECT_NUM))
    {
    	// Listen failed, report error
    	errorreport(SERVER_LISTEN_ERR);
		exit(1);
    }

    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);

	/*  Now wait for clients and requests.
    	Since we have passed a null pointer as the timeout parameter, no timeout will occur.
    	The program will exit and report an error if select returns a value of less than 1.  */

    while(1) 
	{
        char ch[MAX_LINE];
        int fd;
        int nread;

        testfds = readfds;

        printf("server waiting\n");
        result = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval *) 0);

        if(result < 1) 
		{
            perror("server5");
            exit(1);
        }

	/*  Once we know we've got activity,
   	 we find which descriptor it's on by checking each in turn using FD_ISSET.  */

        for(fd = 0; fd < FD_SETSIZE; fd++) 
		{
            if(FD_ISSET(fd,&testfds)) 
			{

	/*  If the activity is on server_sockfd, it must be a request for a new connection
    	and we add the associated client_sockfd to the descriptor set.  */

                if(fd == server_sockfd) 
				{
                    client_len = sizeof(client_addr);
                    client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
                    FD_SET(client_sockfd, &readfds);
                    printf("adding client on fd %d\n", client_sockfd);
                }

	/*  If it isn't the server, it must be client activity.
    	If close is received, the client has gone away and we remove it from the descriptor set.
    	Otherwise, we 'serve' the client as in the previous examples.  */

                else 
				{
                    ioctl(fd, FIONREAD, &nread);

                    if(nread == 0) 
					{
                        close(fd);
                        FD_CLR(fd, &readfds);
                        printf("removing client on fd %d\n", fd);
                    }

                    else 
					{
					
						read(fd, ch, MAX_LINE);
						sleep(5);
                        printf("serving client on fd %d\n", fd);
                        write(fd, ch, MAX_LINE);
                    }
                }
            }
        }
    }
}
