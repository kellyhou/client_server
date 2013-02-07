/*
 *This c file defines file processing functions in server application.
 */


#include "Server.h"
#include "ServerFunc.h"
#include "ServerMacro.h"


// Global variable
const char *originalfilename = "test.txt";
const char *compressedfilename = "test.gz";
const char *no_file = "local file not exist!";
unsigned char *bitmap_buf;
int bitmaplength;





/*-----------------------------------------------------
 *function name: SendFileData
 *input:             socket descriptor
 *output:           status, 0-success, -1-fail
 *This function sends data to client
 *-----------------------------------------------------
 */

int SendFileData(int socket)
{
	int fd;
	int sendlength = 0;
	int i;
	int filechunklength = 0;
	char buf[MAX_LINE];


	// Open the file
	fd = open(compressedfilename, O_RDONLY);
	if(fd < 0)
	{
		// Open file fail, error report
		ErrorReport(FILE_OPEN_ERR);
		return FALSE;
	}	
	
	// Send the file, one chunk at a time
	bzero(buf, MAX_LINE);
	filechunklength = read(fd, buf, MAX_LINE);
	while(filechunklength)
    {
        if(FALSE == filechunklength)
        {	
        	// Read file fail, error report, close file
            ErrorReport(FILE_READ_ERR);
			close(fd);
			return FALSE;
        }

        // Use for loop to make sure the chunck has been sent completely
        for(i = 0; i < filechunklength; i += sendlength)
        {
            sendlength = send(socket, buf + i, filechunklength - i, 0);
	        if(FALSE == sendlength)
	        {
	        	// Send file error, error report, close file
	            ErrorReport(FILE_SEND_ERR);
				close(fd);
				return FALSE;
	        }
        }
		// Fresh buffer, send new chunk
		bzero(buf, MAX_LINE);
	    filechunklength = read(fd, buf, MAX_LINE);
    }              
    close(fd);
           
   return TRUE;
        
}

/*-----------------------------------------------------
 *function name: FileCompress
 *input:             void
 *output:           status, 0-success, -1-fail
 *This function compresses original file to a compressed file
 *-----------------------------------------------------
 */

int FileCompress(void)
{
	int fd;
	gzFile fc;
	char compressedbuf[MAX_LINE];
	int compresslength;
	int writelength;

	// Open original file
	fd = open(originalfilename, O_RDONLY);
	// Open and create a compressed file to save compressed content
    fc = gzopen(compressedfilename, "wb");

	// Compress file, one chunk at a time
	bzero(compressedbuf, MAX_LINE);
	compresslength = read(fd, compressedbuf, MAX_LINE);
	while(compresslength)
    {
        if(FALSE == compresslength)
        {
        	// Read original file fail, error report, close files
            ErrorReport(FILE_READ_ERR);
			close(fd);
			gzclose(fc);
			return FALSE;
        }

        writelength = gzwrite(fc, compressedbuf, compresslength);
        if(FALSE == writelength)
        {
        	// Write compressed file fail, error report, close files
            ErrorReport(FILE_WRITE_ERR);
			close(fd);
			gzclose(fc);
			return FALSE;
        }
		// Fresh buffer, compress new chunk
		bzero(compressedbuf, MAX_LINE);
	    compresslength = read(fd, compressedbuf, MAX_LINE);
    }  
    close(fd);
	gzclose(fc);
	
	return TRUE;
}

/*-----------------------------------------------------
 *function name: GetFileSize
 *input:             file name
 *output:           file size
 *This function get the file size
 *-----------------------------------------------------
 */

int GetFileSize(const char *file_name) 
{
       struct stat buf;
       if (stat(file_name, &buf) != 0 ) 
       		return 0;
       return( buf.st_size );
}

/*------------------------------------------------------------------
 *function name: ReceiveFileStatus
 *input:             socket descriptor
 *output:           file status  1 - no file in client, 0 -file needs update, -1 - error
 *This function receive local file status: 
 * If no file or file is small, send whole file to client
 * If file needs update, check MD5 and compose bitmap information
 *------------------------------------------------------------------
 */

int ReceiveFileStatus(int socket)
{
	int fd;
	int i,j;
	int readlength, recvlength, sendlength;
	char buf[MAX_LINE];
	char recvbuf[MAX_LINE];
	int file_size;
	char md5buf[16];
	int bitmapindex = 0;
	int bitindex = 7;
	int result;
	int clientmd5length, restmd5length;
	int serverchunknum;

	// Get file size
	file_size = GetFileSize(originalfilename);
	printf("File size = %u\n", file_size);
	serverchunknum = (file_size / MAX_LINE) + 1;
	printf("Server chunk num = %d\n", serverchunknum);
	// Calculate bitmap length
	bitmaplength = ((file_size / MAX_LINE) / 8) + 1;
	printf("bitmaplength is %d\n", bitmaplength);
	bitmap_buf = malloc(bitmaplength);
	bzero(bitmap_buf, bitmaplength);
	

	// Open the file
	fd = open(originalfilename, O_RDONLY);
	if(fd < 0)
	{
		// Open file fail, error report
		ErrorReport(FILE_OPEN_ERR);
		free(bitmap_buf);
		return FALSE;
	}
	
	// Get file status
	bzero(recvbuf, MAX_LINE);
	recvlength = recv(socket, recvbuf, MAX_LINE, 0);
	if(TRUE== (strcmp(recvbuf, no_file)))
	{
		printf("Send the whole file...\n");
		// If no file or file is small, send whole compressed file
		result = SendFileData(socket);
		if(FALSE == result)
		{
			// Send file error, error report happened in SendFileData function, just exit here
			free(bitmap_buf);
			return FALSE;
		}
		
		return 1;
	}
	else
	{
		// If file needs update, check MD5 value and compose bitmap 
		
		//MD5_length = (recvbuf[0] | (recvbuf[1] << 8) | (recvbuf[2] << 16) | (recvbuf[3] << 24));
		clientmd5length = atoi(recvbuf);
		restmd5length = clientmd5length;
		printf("Received MD5 length = %d\n", clientmd5length);
		
		printf("Send bitmap length....\n");
		sendlength = send(socket, &bitmaplength, sizeof(int), 0);
		if(FALSE == sendlength)
		{
			ErrorReport(SEND_DATA_ERR);
			free(bitmap_buf);
			return FALSE;
		}
		
		// Begin to receive MD5 data, once a chunk
		printf("Begin to receive MD5 data....\n");
		bzero(recvbuf, MAX_LINE);
		recvlength = recv(socket, recvbuf, MAX_LINE, 0);
		while(recvlength)
		{
			printf("Begin to check MD5 diff.... recvlength = %d\n", recvlength);
			if(FALSE == recvlength)
			{
				// Receive MD5 value fail, error report, close files
				ErrorReport(RECV_DATA_ERR);
				close(fd);
				free(bitmap_buf);
				return FALSE;
			}
			
			for( i = 0; i < (recvlength / 16); i++)
			{
				// Read file content
				readlength = read(fd, buf, MAX_LINE);
				if(FALSE == readlength)
				{
					// read original file fail, error report, close files
					ErrorReport(FILE_READ_ERR);
					close(fd);
					free(bitmap_buf);
					return FALSE;
				}
				
				// Calculate MD5 and check value
				MD5((unsigned char*)buf, readlength, (unsigned char*)md5buf);
				for(j = 0; j < 16; j++)
				{
					if(md5buf[j] != recvbuf[j + (i * 16)])
					{	
						// If MD5 mismatch, record in bitmap
						*(bitmap_buf + bitmapindex) |= (0x1<<bitindex); 
						break;
					}
				}
				
				bitindex -= 1;
				if(-1 == bitindex)
				{
					bitindex = 7;
					bitmapindex++;
				}				
			}
			// Fresh buffer, receive new chunk
			bzero(recvbuf, MAX_LINE);
			// If get enough MD5 data, break
			restmd5length -= recvlength;
			if(restmd5length == 0)
			{
				printf("Receive enough MD5 Data\n");
				break;
			}
			recvlength = recv(socket, recvbuf, MAX_LINE, 0);
		}
		
		// If file in server > file in client, rest of bitmap should be 1
		for(i = ((clientmd5length / 16) + 1); i <= serverchunknum; i++)
		{
			*(bitmap_buf + bitmapindex) |= (0x1<<bitindex); 
			bitindex -= 1;
			if(-1 == bitindex)
			{
				bitindex = 7;
				bitmapindex++;
			}
		}		
		
		// Send bitmap information
		printf("Send bitmap information....\n");
		for(i = 0; i < bitmaplength; i++)
		{
			printf("%8x ", bitmap_buf[i]);
			if( i == (bitmaplength - 1))
				printf("\n");
		}		
		printf("\n");
		sendlength = send(socket, bitmap_buf, bitmaplength, 0);
		if(FALSE == sendlength)
		{
			ErrorReport(SEND_DATA_ERR);
			close(fd);
			free(bitmap_buf);
			return FALSE;
		}
		close(fd);	
		return TRUE;
	}
}

/*---------------------------------------------
 *function name: SendUpdateData
 *input:             socket descriptor
 *output:           status  0 - success, -1 - error
 *This function send file chunks need be updated
 *---------------------------------------------
 */

int SendUpdateData(int socket)
{
	int fd;
	int i,j;
	int readlength, sendlength;
	char buf[MAX_LINE];
	char updatestatus = 0;

	// Open file
	fd = open(originalfilename, O_RDONLY);
	if(fd < 0)
	{
		// Open file fail, error report
		ErrorReport(FILE_OPEN_ERR);
		free(bitmap_buf);
		return FALSE;
	}

	// Get bitmap information, decide which chunk needs be sent
	printf("Send update data....\n");
	for(i = 0; i < bitmaplength; i++)
	{
		for(j = 7; j >= 0; j--)
		{
			updatestatus = (*(bitmap_buf + i)>>j) & 0x1;
			printf("%d ", updatestatus);
			if(j == 0)
			{
				printf("\n");
			}
			if(updatestatus == 1)
			{
				lseek(fd, ((i * 8) + (7 - j))* MAX_LINE, SEEK_SET);
				readlength = read(fd, buf, MAX_LINE);
				if(FALSE == readlength)
				{
					// read original file fail, error report, close files
					ErrorReport(FILE_READ_ERR);
					close(fd);
					free(bitmap_buf);
					return FALSE;
				}
				printf("send=%d ", readlength);
				sendlength = send(socket, buf, readlength, 0);
				if(FALSE == sendlength)
				{
					ErrorReport(SEND_DATA_ERR);
					close(fd);
					free(bitmap_buf);
					return FALSE;
				}
			}
			
		}
	}
	printf("\nUpdate data send done!\n");
	free(bitmap_buf);
	close(fd);
	
	return TRUE;
}
