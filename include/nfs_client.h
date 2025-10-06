/* Header file for nfs_client. nfs_client is the program that is used by 
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
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include "../include/nfs.h"

#pragma once

/* Function that deals with worker_threads and executes the PUSH/PULL/LIST 
 * requests
 */
void* provide_service(void* arg_list); 

