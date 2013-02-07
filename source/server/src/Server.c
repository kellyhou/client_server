/*
 *This c file is main function of server.
 */
 

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
	char buf[MAX_LINE];
	int pid;
	int opt = 1;



	//  Create a socket for the server.
	if(FALSE == (server_sockfd = socket(AF_INET,SOCK_STREAM,0)))
    {
        // Create socket failed, error report
        ErrorReport(GET_SOCKET_ERR);
        exit(1);
    }
	// Fill socket information
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    server_len = sizeof(server_addr);
	
	// Reuse same port 
	setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Socket bind
    if(FALSE == (bind(server_sockfd,(struct sockaddr *)&server_addr ,sizeof(server_addr)))) 
    {
        // Socket bind failed, error report 
        ErrorReport(SOCKET_BIND_ERR);
        exit(1);
    }

	// Create a connection queue and initialize readfds to handle input from server_sockfd.
    if(FALSE == listen(server_sockfd, MAX_CONNECT_NUM))
    {
    	// Listen failed, report error
    	ErrorReport(SERVER_LISTEN_ERR);
		exit(1);
    }

	// Compress file
	result = FileCompress();
	if(FALSE == result)
	{
		// Compress file fail, error report happened in FileCompress function, just exit here
	    exit(1);
	}


	//  Now wait for clients and requests.
    while(1) 
	{
		// Accept a client
		printf("server waiting......\n");
        client_len = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        // create child process to send file
		pid = fork();
		if(pid < 0)
		{
			// Create child process fail, error report
			ErrorReport(FORK_ERR);
			exit(1);
		}
		else if(pid == 0)
		{
			close(server_sockfd);
	        printf("accepting client on fd %d\n", client_sockfd);
	        // Receive file status from client, decide how to transfer file
			result = ReceiveFileStatus(client_sockfd);
			if(FALSE == result)
	        {	
	        	// Receive file status fail, exit
	            exit(1);
	        }
			else if(1 == result)
			{
				// No file or file is too small, send whole compressed file
				exit(0);
			}
			else
			{
				//Server send file update bitmap, get the ACK and send data update
				// Send compressed file to client
				result = recv(client_sockfd, buf, MAX_LINE, 0);
				if(FALSE == result)
				{
					// Receive ACK error, free memory, error report
					ErrorReport(RECV_DATA_ERR);
					free(bitmap_buf);
					exit(1);
				}
				// Send file update chunks
				result = SendUpdateData(client_sockfd);
				if(FALSE == result)
				{
					// Send fail, exit
					exit(1);
				}
				printf("processing done, close client on socket %d\n", client_sockfd);
				if(FALSE == close(client_sockfd))
				{
					// Close socket err, error report
					ErrorReport(CLOSE_SOCKET_ERR);
					exit(1);
				}
				exit(0);
			}
			
		}
		else
		{
			if(FALSE == close(client_sockfd))
			{
				// Close socket err, error report
				ErrorReport(CLOSE_SOCKET_ERR);
				exit(1);
			}
		}

    }
}
