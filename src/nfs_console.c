/* Source file for nfs_console. nfs_console is the program that is responcible
 * for communicating with the user through a prompt, and redirect user's 
 * requests to nfs_manager. The program is executed as:
 *      ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>
 *
 * The above parameters are explained bellow:
 *      - <console-logfile>: A file that the program will use to write logs
 *
 *      - <host_IP>: The name of the server that hosts nfs_manager
 *
 *      - <host_port>: The port number nfs_manager is running
 *  
 *  After starting the program, the user is provided with a prompt, in which he
 *  can run the following commands:
 *
 *      - add <source> <target>: Adds the new pair for syncing
 *
 *      - cancel <source>: Cancels the source syncing
 *
 *      - shutdown: Shuts down the program
 *
 *  nfs_console provides nfs_manager with the given commands and outputs to user 
 *  the result 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include "../include/nfs.h"

#define SOCKET 0
#define STDIN 1

int main(int argc,char* argv[]) {
    // Parsing our arguments
    if (argc != 7) {
        fprintf(stderr,"ERROR! Wrong number of arguments given\n");
        exit(-1);
    }
    char* filename = "";
    char* host = "";
    int host_port = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-l"))
            filename = argv[++i];
        else if (!strcmp(argv[i],"-h"))
            host = argv[++i];
        else if (!strcmp(argv[i],"-p"))
            host_port = atoi(argv[++i]);
        else {
            fprintf(stderr,"ERROR! Wrong parameters given\n");
            exit(-1);
        }
    }
    if (filename[0] == '\0' || host[0] == '\0' || host_port == 0){
        fprintf(stderr,"ERROR! Wrong parameters given\n");
        exit(-1);
    }
    // Opening our logfile 
    FILE* logfile = fopen(filename,"w");
    if (logfile == NULL)
        perror_exit("ERROR! fopen failed\n");

    int sockfd = connect_to_host(host, host_port);
    if (sockfd < 0)
        perror_exit("ERROR! connect_to_host failed\n");

    char action[1024];
    char source_dir[1024];
    char target_dir[1024];
    char output[1024];
    char time_buffer[1024];
    int n;
    // We will use poll, to deal with the two different input streams (socket and stdin)
    struct pollfd input_fds[2];
    input_fds[SOCKET].fd = sockfd;
    input_fds[SOCKET].events = POLLIN;
    input_fds[STDIN].fd = 0; // stdin
    input_fds[STDIN].events = POLLIN;
    printf("Enter your commands:\n");
    do { 
        if (poll(input_fds,2,0) > 0) {
            if (input_fds[SOCKET].revents == POLLIN) {
                // We read nfs_manager's output from our socket and print it to 
                // the user
                n = read(sockfd,output,1024);
                if (n < 0) 
                    perror_exit("ERROR! failed to read from socket");
                
                if (n > 0) {
                    write(1,output,n);
                    write(1,"\n",1);
                }    

            }
            else if (input_fds[STDIN].revents == POLLIN) {
                // We making them empty, so we can print them even when a 
                // command doesn't require them
                source_dir[0] = '\0';
                target_dir[0] = '\0';
                scanf("%s",action);
                if (!strcmp(action,"add")) {
                    scanf("%s",source_dir);
                    scanf("%s",target_dir);
                }
                else if (!strcmp(action,"cancel")) {
                    scanf("%s",source_dir);
                }
                // We entered a wrong command. No need to send to nfs_manager
                else if (strcmp(action,"shutdown")) {
                    printf("Wrong command give\n");
                    continue;

                }
                // Write input in socket
                char buff[1024];
                sprintf(buff,"%s %s %s\n",action,source_dir,target_dir);
                write(sockfd,buff,strlen(buff));
                // Write input in logfile
                fprintf(logfile,"[%s] Command %s %s %s\n",print_timestamp(time_buffer),action,source_dir,target_dir);
            }
        }
         
    } while (strcmp(action,"shutdown"));

    // Closing logfile and socket
    fclose(logfile);
    close(sockfd);
}
