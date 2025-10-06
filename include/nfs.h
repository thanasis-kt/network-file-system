/* This header file contains functions that can be used by all different 
 * programs of the app, in order to organize our code better
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#pragma once


/* Puts the current timestamp inside time_buffer and returns a pointer to it */
char* print_timestamp(char time_buffer[32]); 

/* Prints the error message using perror and exits the program with error code -1 */
void perror_exit(char* error_msg);

/* Puts n's decimal representation in buffer, and returns a pointer to buffer. 
 * !!! It doesn't allocate memory for buffer !!!
 */
char* number_to_string(char* buffer,int n);

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

int decode_format(const char* input,char* dir_name,char* host_addr,int* port);

/* Reads the next word in our socket stream.  
 * It reads characters in a loop and puts them in dir buffer until a space
 * character is read. At the end it also appends '\0' character 
 * !!! This function doesn't allocate space for dir buffer it should be already
 *     done by its caller !!!
 *
 */
void getnextword(int sockfd,char* dir);

/* This function reads a numbers from a socket until whitespace is given and 
 * returns the number that was read
 *
 * In case of an error, it returns INT_MIN
 */
int getsize(int sockfd);

/* Attemps to connect to <host> in port <port>. At success it returns a socket
 * we can use for communicating with host, else -1. 
 *
 * If the function returns -1, then the errno is set accordingly by the syscall
 * that failed
 */
int connect_to_host(char* host,int port);


