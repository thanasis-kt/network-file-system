/* Header file for nfs_manager program. nfs_manager is the program that is 
 * responsible for sychronizing, all other programs of the app, through sockets
 * in order to accieve the directory synchronization.
 * nfs_manager is executed as follows:
 *
 *      ./nfs_manager -l <manager_logfile> -c <config_file> -n <worker_limit> 
 *          -p <port_number> -b <bufferSize>
 *
 *  Each parameter is described below:
 *
 *  manager_logfile: the file that will be used by nfs_manager to print it's 
 *  logfiles
 *
 *  config_file: the config file that specifies a set of directories, that the 
 *  nfs_manager will synchronize at start
 *
 *  worker_limit: the maximum number of threads used for synchronizing processes
 *
 *  port_number: the port that nfs_manager will run
 *
 *  bufferSize: number of slots of a buffer that will keep syncing processes 
 *  that will be executed by worker_threads
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "nfs.h"
#include "map.h"

#pragma once

// Structure that contains variables, used to access our thread buffer
typedef struct {
    char** buffer; // Size is given at end
    int start;
    int end;
    int count;
} pool_t;



/* A worker_thread implements the syncing process between different nfs_clients.
 * It takes an action of the form <filename> <source client> <target client> 
 * from the pool_t buffer and connects to source and target client.
 */
void* worker_thread(void* args);

/* Places an action in worker's buffer. This function uses condition variables 
 * and mutexes, to deal with problems like racing and a full buffer
 */
void place(pool_t* pool,char* action);

/* Obtains an action from worker's buffer and copies it in action. This function
 * uses condition variables and mutexes, to deal with problems like racing and 
 * an emtpy buffer
 */
void obtain(pool_t* pool,char* action);

/* Adds a pair for sychronization, by doing the following:
 *      - Starts a connection with source's nfs_client in the specified port
 *      - Sends LIST command to source's nfs_client, to obtain source_dir's 
 *        files
 *      - For every file in source_dir, adds a sync request in worker's queue
 *
 *  
 *  This function acts as the producer in our consumer-producer approach to 
 *  worker-threads synchronization. It uses mutexes and condition variables to
 *  deal with the danger of racing conditions.
 *
 *  Returns 0, or else -1 in case of an error.
 *
 * */
int add_pair(char* source,char* target,int console_sock);



/* This function writes a record in manager's log in the form:
 *
 *  [TIMESTAMP] [SOURCE_DIR] [TARGET_DIR] [THREAD_PID] [OPERATION] [RESULT] [DETAILS]
 *
 *  when a worker finishes a given action.
 */

void write_worker_result(int logfile_fd,char* source_dir,char* target_dir,char* operation,char* result,char* details);

/* This function writes the given message using write, and if write fails, it 
 * appends an error message, for the reason of fail in error_buffer. 
 * This function will be used by worker threads, to reduce the number of 
 * conditional branches that check for fails of syscalls, as worker threads 
 * are using a lot of write syscalls */
void write_and_check(int fd,char* buf,int len,char* error_buffer);
