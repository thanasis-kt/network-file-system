/* This source file contains functions that can be used by all different 
 * programs of the app, in order to organize our code better
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "../include/nfs.h"



/* Puts the current timestamp inside time_buffer and returns a pointer to it */
char* print_timestamp(char time_buffer[32]) {
    time_t tm;
    struct tm* time_info;
    time(&tm);
    time_info = localtime(&tm);
    strftime(time_buffer, 32, "%Y-%m-%d %H:%M:%S", time_info);
    return time_buffer;
}

/* Prints the error message using perror and exits the program with error code -1 */
void perror_exit(char* error_msg) {
    perror(error_msg);
    exit(-1);
}

/* Puts n's decimal representation in buffer, and returns a pointer to buffer. 
 * !!! It doesn't allocate memory for buffer !!!
 */
char* number_to_string(char* buffer,int n) {
    int i = 0;
    if (n == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }
    while (n > 0) {
        buffer[i++] = n % 10 + '0';
        n /= 10;
    }
    buffer[i] = '\0';
    // We put the number in reverse, we need to reverse it
    i--;
    for (int j = 0; j < i; j++) {
        int temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
        i--;
    }
    return buffer;
}

/* Decodes the following string format:
 *      <directory_name>@<host_addr>:<port_number>
 *  that is given in input argument, where each value means:
 *
 *      <directory_name>: The name of the directory given
 *
 *      <host_addr>: The ipv4 address of the given host 
 *
 *      <port_number>: The port number the host is running it's service at
 *
 *  Return Value:
 *      In success returns 1 and arguments dir_name,host_addr and port have the
 *      decoded value in them, or if it fails -1
 *
 *  !!! The strings dir_name,host_addr aren't allocated by this function, it 
 *      should be done by the caller !!!
 */

int decode_format(const char* input,char* dir_name,char* host_addr,int* port) {
    char str[1024];
    strcpy(str,input);
    char* tmp = strtok(str,"@");
    // str is not in the correct format
    if (tmp[0] == '\0')
        return -1;

    strcpy(dir_name,tmp);

    tmp = strtok(NULL,":");
    if (tmp[0] == '\0')
        return -1;

    strcpy(host_addr,tmp);

    tmp = strtok(NULL,"\0");
    if (tmp[0] == '\0')
        return -1;

    *port = atoi(tmp);
    return 0;
}

/* Returns the next word in our socket stream.
 * It reads characters in a loop and puts them in file buffer until a space
 * character is read. At the end it also appends '\0' character 
 * !!! This function doesn't allocate space for file buffer it should be already
 *     done by its caller !!!
 *
 */
void getnextword(int sockfd,char* file) {
    char buf[1];
    // We skip whitespace
    do {
        buf[0] = ' ';
        if (read(sockfd,buf,1) < 0)
            perror_exit("ERROR! Read failed\n");

    } while (isspace(buf[0]));
    file[0] = buf[0];
    int i = 1;
    // We read until we encounter a white space
    while (read(sockfd,buf,1) > 0) {
        if (isspace(buf[0]))
            break;
        file[i++] = buf[0];
    }
    file[i] = '\0';
}

/* This function reads a numbers from a socket until whitespace is given and 
 * returns the number that was read
 *
 * In case of an error, it returns INT_MIN
 */
int getsize(int sockfd) {
    int num = 0;
    char buf[1];
    int sign = 1;
    buf[0] = ' ';
    // We skip whitespace
    do {
        if (read(sockfd,buf,1) < 0)
            perror_exit("ERROR! Read failed\n");
    } while (isspace(buf[0]));
    if (buf[0] == '-') {
        sign = -1;
    }
    else if (!isdigit(buf[0]))
        return INT_MIN; // An error occured

    else 
        num = buf[0] - '0';
    while (read(sockfd,buf,1) > 0) {
        // Full number given
        if (isspace(buf[0]))
            return sign * num;
        else if (isdigit(buf[0]))
            num = num * 10 + buf[0] - '0';
        // Wrong character given 
        else  
            return INT_MIN; // An error_occured
        buf[0] = ' ';
    }
    return sign * num;
}

/* Attemps to connect to <host> in port <port>. At success it returns a socket
 * we can use for communicating with host, else -1. 
 *
 * If the function returns -1, then the errno is set accordingly by the syscall
 * that failed
 */
int connect_to_host(char* host,int port) {

        struct sockaddr_in client;
        struct sockaddr* sourceptr = (struct sockaddr*)&client;
        struct hostent* rem;
        int sock = socket(AF_INET,SOCK_STREAM,0);
        if (sock < 0)
            return -1;
        
        // Puting the values to sockaddr before calling connect 
        client.sin_family = AF_INET;
        client.sin_port = htons(port);
        // Get source's host ip address
        if ((rem = gethostbyname(host)) == NULL) 
            return -1;

        memcpy(&(client.sin_addr),rem->h_addr,rem->h_length);

        if (connect(sock,sourceptr,sizeof(client)) < 0)
            return -1;

        return sock;
}
