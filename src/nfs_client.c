/* Source file for nfs_client. nfs_client is the program that is used by 
 * nfs_manager, for synchronizing files.The program is connected by a socket to 
 * nfs_manager, and communicates with him through TCP protocol. nfs_client 
 * offers the following options:
 *      - LIST source_dir: Sends to the host the files contained inside 
 *                         source_dir. At the end of the message it sends
 *                         the character '.'
 *
 *      - PULL /source_dir/file.txt: Sends to the host the contents of
 *                         ./source_dir/file.txt (Paths are relative due
 *                         to security concerns), with the following format:
 *                         <filesize><space><data...>
 *                          If filesize is -1 then data will contain the ERROR
 *                          occured

 *      - PUSH /target_dir/file.txt chunk_size data: Reads from host the data
 *                          sent, and appends it to /target/file.txt. if 
 *                          chunk_size is -1 then we clear the file first and 
 *                          if chunk_size is 0, then we have read all the data
 *                          given, and we can close the file
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include "../include/nfs.h"
#include "../include/nfs_client.h"
#define MOD 0777

int main(int argc,char* argv[]) {
    // Parsing arguments
    int port;
    if (argc != 3) {
        fprintf(stderr,"ERROR! Wrong number of arguments given\n");
        exit(-1);
    }

    port = atoi(argv[2]);
    if (port == 0) {
        fprintf(stderr,"ERROR! Wrong value given as port number <%s>\n",argv[2]);
        exit(-1);
    }
    pthread_t thread; // Will be used to create threads to serve nfs_manager

    // Creating our socket
    int sockfd; 
    if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        perror_exit("ERROR! Socket failed\n");

    // Binding our socket to the specified port
    struct sockaddr_in server; // We use AF_INET so we use this sockaddr
    struct sockaddr* serverptr = (struct sockaddr*)&server;
    struct sockaddr_in client; // The client is nfs_manager
    struct sockaddr* clientptr = (struct sockaddr*)&client;
    socklen_t clientlen = sizeof(client);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if (bind(sockfd,serverptr,sizeof(server)) < 0)
        perror_exit("ERROR! bind failed\n");
    
    // Listening for connections
    if (listen(sockfd,5) < 0) 
        perror_exit("ERROR! listen failed\n");


    while (1) {
        int newsock;
        // Accepting conection
        if ((newsock = accept(sockfd,clientptr,&clientlen)) < 1)  {
            perror_exit("ERROR! accept failed\n");
        }    
        pthread_create(&thread,NULL,provide_service,(void*)newsock);
        pthread_detach(thread); 
    }
}


void* provide_service(void* arg) {
    int sockfd = (int)arg; 
    int fd; // The file descriptor we will use in our PUSH/PULL actions
    char buffer[1024];
    char action[6]; // The action we want to perform
    
    getnextword(sockfd,action);

    if (!strcmp(action,"LIST")) {
        char dir[255]; 
        // Reading the name of our directory using helping function
        getnextword(sockfd,dir);
        // All directories are in the form /dir_name
        DIR* dir_ptr = opendir(dir + 1); 
        if (dir_ptr == NULL) {
            // Sent the halting character "." to client
            write(sockfd,".\n",2);
            close(sockfd);
            return NULL;
        }
        struct dirent* direntp;
        while ((direntp = readdir(dir_ptr)) != NULL) {
            // We skip . and .. directories
            if (strcmp(direntp->d_name,".") == 0 || strcmp(direntp->d_name,"..") == 0)
                continue;
            write(sockfd,direntp->d_name,strlen(direntp->d_name));
            write(sockfd,"\n",1);
        }
        write(sockfd,".\n",2);
    }
    else if (!strcmp(action,"PULL")) {
        char filename[20];
        getnextword(sockfd,filename);
        fd = open(filename + 1,O_RDONLY);
        if (fd < 0) {
            write(sockfd,"-1 ",3);
            write(sockfd,strerror(errno),strlen(strerror(errno)));
            close(sockfd);
            return NULL;

        }
        struct stat info; // To obtain file's size
        if (stat(filename + 1,&info) < 0) {
            // Sent -1 error_message
            write(sockfd,"-1 ",3);
            write(sockfd,strerror(errno),strlen(strerror(errno)));
            close(sockfd);
            return NULL;
        }
        // Print size to socket
        char number_buffer[32];
        number_to_string(number_buffer, info.st_size);
        write(sockfd,number_buffer,strlen(number_buffer));
        write(sockfd," ",1);

        // Print the contents of the file to the socket
        int n;
        char buff[1024];
        int count = 0;
        while ((n = read(fd,buff,1024)) > 0) {
            if (write(sockfd,buff,n) < n) {
                close(fd);
                close(sockfd);
                return NULL;
            }
            count += n;
        }
        close(fd);
    }
    else if (!strcmp(action,"PUSH")) {
        char filename[20];
        int chunk_size;
        bool halt = false;
        // Loop is terminated from inside using break
        while (true) {
            getnextword(sockfd,filename);
            chunk_size = getsize(sockfd);

            switch (chunk_size) {
            case INT_MIN:
                // Wrong characters given as chunk_size
                close(sockfd);
                return NULL;
            case -1:
                // We can close the file, PUSH stream ended
                if (close(fd) < 0) {
                    perror("ERROR! close failed\n");
                    exit(-1);
                }
                halt = true;
                break;
            case 0:
                fd = open(filename + 1,O_CREAT | O_WRONLY | O_TRUNC,MOD);
                if (fd < 0) {
                    close(sockfd);
                    return NULL;
                }
                break;
            default:
                // We write the data to the file
                do {
                    // We read the maximum amount of data we can fit in the buffer.
                    // That will either be all data sent  or buffer's capacity
                    int n = read(sockfd,buffer,(1024 < chunk_size) ? 1024 : chunk_size);
                    write(fd,buffer,n);
                    chunk_size -= n;
                } while (chunk_size > 0);
            }    
            // We are done with the PUSH action
            if (halt)
                break;
            getnextword(sockfd,action);
            action[5] = '\0';
            // We expected PUSH
            if (strcmp(action,"PUSH")) {
                break;
            }

        }
    }
    close(sockfd);
    pthread_exit(NULL);
}

