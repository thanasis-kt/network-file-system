/* source file for nfs_manager program. nfs_manager is the program that is 
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
#include <stdbool.h>
#include <threads.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "../include/nfs.h"
#include "../include/map.h"
#include "../include/nfs_manager.h"

int logfile_fd; 

int worker_limit = 5;

pthread_mutex_t log_mtx; // Mutex that locks write on our logfile
                         // This mutex doesn't need to be asossiated with any 
                         // condition variables, because we can write anytime
                         // we want to the logfile (it will be closed after 
                         // all threads join)

pthread_mutex_t buffer_mtx; // Mutex that locks access to our pool_t structure


int buffer_size = 0; // The number of slots in our buffer pool

// Our condition variables (all on buffer_mtx)
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

// This variable contains the worker's buffer and will be used by obtain and 
// place functions
pool_t pool;


int main(int argc,char* argv[]) {
    // Parsing arguments
    char* logfile = "";
    char* config_file = "";
    int port_number = 0;
    // Our arguments are either 9 or 11 (-n <number of workers> can be excluded)
    if (argc != 11 && argc != 9) {
        fprintf(stderr,"ERROR! Wrong number of arguments given\n");
        exit(-1);
    }

    // We iterate through [1,argc -1] because that's where our flags should be
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i],"-l"))
            logfile = argv[++i]; 
        else if (!strcmp(argv[i],"-c"))
            config_file = argv[++i];
        else if (!strcmp(argv[i],"-n")) 
            worker_limit = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-p"))
            port_number = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-b"))
            buffer_size = atoi(argv[++i]);

        // Wrong type of argument
        else {
            fprintf(stderr,"ERROR! Wrong value given as argument\n");
            exit(-1);
        }
    }
    // Check if all values are given, and if they are correct

    // Atoi returns 0 on error
    if (port_number * worker_limit * buffer_size == 0) {
        fprintf(stderr,"ERRROR! Wrong value given as argument\n");
        exit(-1);
    }

    // Opening our files
    logfile_fd = open(logfile,O_WRONLY | O_CREAT | O_TRUNC);
    FILE* conf_input = fopen(config_file,"r");
    if (logfile_fd < -1 || conf_input == NULL)
        perror_exit("ERROR! fopen failed\n");

    // Initializing our sync_info_mem_store structure
    map* mem = map_create();

    // Time buffer will be used by print_timestamp
    char time_buffer[1024];

    // Initializing our thread management variables
    pthread_t* workers = malloc(sizeof(pthread_t) * worker_limit);
    if (workers == NULL)
        perror_exit("ERROR! malloc failed\n");

    pool.start = 0;
    pool.end = -1;
    pool.count = 0;

    pool.buffer = malloc(sizeof(char*) * buffer_size);
    if (pool.buffer == NULL)
        perror_exit("ERROR! malloc failed\n");

    for (int i = 0; i < buffer_size; i++) {
        pool.buffer[i] = malloc(sizeof(char) * 1024);
        if (pool.buffer[i] == NULL)
            perror_exit("ERROR! malloc failed\n");
    }

    //Initializing our mutexes and condition variables
    pthread_mutex_init(&buffer_mtx,NULL);
    pthread_mutex_init(&log_mtx,NULL);
    pthread_cond_init(&cond_nonfull,NULL);
    pthread_cond_init(&cond_nonempty,NULL);

    for (int i = 0; i < worker_limit; i++) {
        // Creating our worker threads
        pthread_create(&workers[i],NULL, worker_thread,NULL);
    }

    // We are ready to start a connection with nfs_console and start syncing files

    // nfs_manager is the server and nfs_console is the client
    struct sockaddr_in server,client;
    struct sockaddr* serverptr = (struct sockaddr*)&server;
    struct sockaddr* clientptr = (struct sockaddr*)&client;
    
    // Creating a socket
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0)
        perror_exit("ERROR! socket failed\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port_number);
    // Binding our socket to given port (parameter)
    if (bind(sockfd,serverptr,sizeof(server)) < 0)
        perror_exit("ERROR! bind failed\n");

    // Listening for connections
    if (listen(sockfd,5) < 0)
        perror_exit("ERROR! listen failed\n");

    socklen_t clientlen;
    int console_sock;

    // Starting interaction with nfs_console

    // Accepting connection from nfs_console
    if ((console_sock = accept(sockfd,clientptr,&clientlen)) < 0)
        perror_exit("ERROR! accept failed\n");

    // We are ready to sync the pairs that are in the config file
    
    while (!feof(conf_input)) {
        // Reading from conf_input the source and target pair
        char source[1024],target[1024];
        fscanf(conf_input,"%s %s",source,target); 
        if (feof(conf_input))
            break; 
        if (add_pair(source,target,console_sock) == 0) {
            // Putting all decoded values in map
            map_add(mem,source,target);
        }   
        else {
            dprintf(console_sock,"[%s] Failed to add pair: %s %s\n",print_timestamp(time_buffer),source,target);
        }

     }

    // Starting interaction with nfs_console
    char command[1024];
    char *action,*source,*target;
    // We will read from console sock and use strtok_r to decode the given 
    // command. If at any point of using strtok_r we get NULL, we re-read
    // from console socket and re-initialize strtok_r.   int n;
    
    // First we initialize strtok and action
    int n = read(console_sock,command,1024);
    command[n] = '\0';
    char* action_ptr = NULL; // Pointer that will be used by strtok_r
    action = strtok_r(command," \n",&action_ptr);
    char msg[1024];
    int msg_len;
    while (true) { // Shutdown command from nfs_console halts the program
        
        // We need to read new data from console
        while (action == NULL) {
            n = read(console_sock,command,1024);
            command[n] = '\0';
            action_ptr = NULL;
            action = strtok_r(command," \n",&action_ptr);
        }    
        if (!strcmp(action,"add")) {
            source = strtok_r(NULL," \n",&action_ptr);
            // We need to read new data from console
            while (source == NULL) {
                n = read(console_sock,command,1024);
                command[n] = '\0';
                action_ptr = NULL;
                source = strtok_r(command," \n",&action_ptr);
            }
            target = strtok_r(NULL," \n",&action_ptr);
            // We need to read new data from console
            while (target == NULL) {
                n = read(console_sock,command,1024);
                command[n] = '\0';
                action_ptr = NULL;
                target = strtok_r(command," \n",&action_ptr);
            }

            // Putting all files of source for syncing with target, if they
            // aren't already syncing
            dir_info* info = map_find(mem,source);
            if (info == NULL || info->is_active == false) {
                // At first we remove the pair if is already in map
                map_remove(mem,source);

                // Putting files in worker's buffer
                if (add_pair(source,target,console_sock) == 0) {
                     // Putting pair in map
                    map_add(mem,source,target);
                }
                else {
                    dprintf(console_sock,"[%s] Failed to add pair: %s %s\n",print_timestamp(time_buffer),source,target);
                }

            }
            // pair already in queue
            else {
                dprintf(console_sock,"[%s] Already in queue: %s\n",print_timestamp(time_buffer),source);
            }

        }
        else if (!strcmp(action,"cancel")) {
            source = strtok_r(NULL," \n",&action_ptr);
            // We need to read new data from console
            while (source == NULL) {
                read(console_sock,command,1024);
                command[n] = '\0';
                action_ptr = NULL;
                source = strtok_r(command," \n",&action_ptr);
            }
            dir_info* info = map_find(mem,source);
            if (info == NULL || info->is_active == false) {
                dprintf(console_sock,"[%s] Directory not being synchronized: %s\n",print_timestamp(time_buffer),source);
            }
            // The directory is active
            else {
                info->is_active = false;
                // We need to modify the worker's buffer and remove every instance that is still there
                pthread_mutex_lock(&buffer_mtx);
                fprintf(stderr,"CANCELING BUFFER\n");
                int i = pool.start;
                int k = 0;
                while (k < pool.count) {
                    char entry[1024];
                    strcpy(entry,pool.buffer[i]);
                    fprintf(stderr,"Canceling entry %s\n",entry);
                    char* ptr = NULL;
                    char* tmp = strtok_r(entry," ",&ptr);
                    // We want the second entry
                    tmp = strtok_r(NULL," ",&ptr);
                    // If the entry needs to be canceled then we remove it from
                    // the pool buffer, by swapping it with the last item in 
                    // buffer, and making it empty
                    if (!strcpy(tmp,source)) {
                        strcpy(pool.buffer[i],pool.buffer[pool.end]);
                        pool.buffer[pool.end][0] = '\0'; // Mark it as emtpy
                        pool.end--;
                        pool.count--;
                    }
                    // If we swapped with the last item we want to check that item also 
                    // so we don't increace i's value
                    else 
                        i++;

                    k++;
                }
                if (pool.count == 0) {
                    pool.start = 0;
                    pool.end = 0;
                }
                // Releasing the lock
                pthread_mutex_unlock(&buffer_mtx);
                // Writing in logfile,stdout and nfs_console
                sprintf(msg,"[%s] Synchronization stopped for %s\n",print_timestamp(time_buffer),source);
                msg_len = strlen(msg);
                // Using mutex so we avoid race condition when we are writing in logfile
                pthread_mutex_lock(&log_mtx);
                write(logfile_fd,msg,msg_len);
                write(1,msg,msg_len); // stdout
                write(console_sock,msg,msg_len); // console
                pthread_mutex_unlock(&log_mtx);

            }

        }
        else if (!strcmp(action,"shutdown")) {
            // Writing messages to nfs_console stdout
            dprintf(console_sock,"[%s] Shutting down manager...\n",print_timestamp(time_buffer));
            printf("[%s] Shutting down manager...\n",print_timestamp(time_buffer));
            dprintf(console_sock,"[%s] Waiting for all active workers to finish...\n",print_timestamp(time_buffer));
            printf("[%s] Waiting for all active workers to finish...\n",print_timestamp(time_buffer));
            break;
        }
        else {
            // Alert nfs_console that the given command was incorect
            dprintf(console_sock,"[%s] Wrong command given: <%s>\n",print_timestamp(time_buffer),action);
        }

        action = strtok_r(NULL," \n",&action_ptr);
    } 

    dprintf(console_sock,"[%s] Processing remaining queued tasks...\n",print_timestamp(time_buffer));
    // Placing shutdown command, so that the worker knows that he can shutdown
    for (int i = 0; i < worker_limit; i++) {
        place(&pool,"shutdown");
        pthread_cond_signal(&cond_nonempty);
    }

    // Waiting until count is 0
    while(true) {
        pthread_mutex_lock(&buffer_mtx);
        if (pool.count == 0)
            break;
        pthread_mutex_unlock(&buffer_mtx);
    }


    dprintf(console_sock,"[%s] Manager shutdown complete...\n",print_timestamp(time_buffer));
    printf("[%s] Manager shutdown complete...\n",print_timestamp(time_buffer));
    close(logfile_fd);
    return 0;

}

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
 *  This function will be run by our main thread whenever we want to sync a 
 *  directory.
 *
 *  Returns 0, or else -1 in case of an error.
 *
 */
int add_pair(char* source,char* target,int console_sock) {
    char src[1024];
    char time_buffer[32];
    strcpy(src,source);
    char* ptr = NULL; // For strtok_r

    // Decoding the values that are in source string 
    char* source_dir = strtok_r(src,"@",&ptr);
    char* source_host = strtok_r(NULL,":",&ptr);
    char* sp = strtok_r(NULL,"\0",&ptr);
    int source_port = atoi(sp);       

    // Creating a socket, to connect with the clients
    int sockfd = connect_to_host(source_host,source_port);
    if (sockfd < 0) {
        return -1;
    }
    
    char trc[1024];
    strcpy(trc,target);
    ptr = NULL; // For strtok_r

    // Decoding the values that are in target string to print them to logfile
    char* target_dir = strtok_r(trc,"@",&ptr);
    char* target_host = strtok_r(NULL,":",&ptr);
    sp = strtok_r(NULL,"\0",&ptr);
    int target_port = atoi(sp);       



    // Enter LIST command to nfs_client
    char action[1024]; // The action we will put in worker's buffer

    char msg[1024]; // For printing messages
    int msg_len;

    char filename[256];
    dprintf(sockfd,"LIST %s\n",source_dir);

    getnextword(sockfd, filename);
    // For every file in source_dir creating a new action and append it in 
    // worker's buffer, until "." is given as filename
    while (strcmp(filename,".")) {             
        // Create action to place to buffer 
        sprintf(action,"%s %s %s\n",filename,source,target);
        // Write to logfile,nfs_console and to stdout that the file was added
        pthread_mutex_lock(&log_mtx);
        sprintf(msg,"[%s] Added file: %s/%s@%s:%d\n",print_timestamp(time_buffer),target_dir,filename,target_host,target_port);
        msg_len = strlen(msg); 
        write(1,msg,msg_len); // Stdout
        write(logfile_fd,msg,msg_len); // Logfile
        write(console_sock,msg,msg_len);
        pthread_mutex_unlock(&log_mtx);
        place(&pool, action);
        pthread_cond_signal(&cond_nonempty);
        // Get the next file
        getnextword(sockfd,filename);
    }

    return 0;
}


void* worker_thread(void* args) {
    // Our consumer, that implements the synchronization process accross two 
    // hosts (one for target and one for source)
    while (true) {
        char error_buffer[1024]; // Our error buffer, used to append every 
                                 // error we are able to catch

        error_buffer[0] = '\0'; // We initialize it as empty to know whether or
                                // not an error occured when strlen > 0

        char number_buffer[16]; // Buffer that will be used to repressent numbers 
                                // as strings

        char action[1024];
        obtain(&pool,action);
        char* source_ptr; // Pointer that will be used by strtok_r
                             // changing by another thread after signaling
        pthread_cond_signal(&cond_nonfull);

        // We can shutdown
        if (!strcmp(action,"shutdown")) {
            pthread_exit(NULL);
        }

        // At first we break the given action into parts using strtok_r
        char* filename = strtok_r(action," \n",&source_ptr);
        char* source = strtok_r(NULL," \n",&source_ptr);
        char* target = strtok_r(NULL," \n",&source_ptr);

        source_ptr = NULL;
        char* source_file = strtok_r(source,"@",&source_ptr);
        char* source_host = strtok_r(NULL,":",&source_ptr);
        int source_port = atoi(strtok_r(NULL," \n",&source_ptr));

        char* target_ptr; // Pointer that will be used by strtok_r
        char* target_file = strtok_r(target,"@",&target_ptr);
        char* target_host = strtok_r(NULL,":",&target_ptr);
        int target_port = atoi(strtok_r(NULL," \n",&target_ptr));

        // Connecting to source and target hosts

        int source_sock = connect_to_host(source_host, source_port);
        int target_sock = connect_to_host(target_host, target_port);
        int bytes_pulled = 0;
        int bytes_pushed = 0;

        if (source_sock < 0 || target_sock < 0) {
            strcat(error_buffer,strerror(errno));
            strcat(error_buffer,",");
        }
        // Connected to hosts successfully, starting synchronization
        else {
            // We will sent the PULL command to source host and sent the data we 
            // read to target host, using PUSH command
            
            write_and_check(source_sock,"PULL ",5,error_buffer);
            write_and_check(source_sock,source_file,strlen(source_file),error_buffer);
            write_and_check(source_sock,"/",1,error_buffer);
            write_and_check(source_sock,filename,strlen(filename),error_buffer);
            write_and_check(source_sock," ",1,error_buffer);

            char buffer[1024];
            int data_sent = getsize(source_sock);
            // If an error happened in nfs_client
            if (data_sent < 0) {
                strcat(error_buffer,"File: ");
                strcat(error_buffer,filename);
                strcat(error_buffer," ");
                read(source_sock,error_buffer,1024);
            }
            else {

                // Sending PUSH requests to target's nfs_client

                // Sending PUSH filename 0 to notify client to open the file 
                write_and_check(target_sock,"PUSH ",5,error_buffer);
                write_and_check(target_sock,target_file,strlen(target_file),error_buffer);
                write_and_check(target_sock,"/",1,error_buffer);
                write_and_check(target_sock,filename,strlen(filename),error_buffer);
                write_and_check(target_sock," 0\n",3,error_buffer);

                do {
                    // We read the maximum amount of data we can from source file
                    // and sent them in buffer
                    int snt = read(source_sock,buffer,(1024 < data_sent) ? 1024 : data_sent);
                    bytes_pulled += snt;
                    if (snt < 0) {
                        strcat(error_buffer,"read failed ");
                        strcat(error_buffer,strerror(errno));
                        strcat(error_buffer,",");
                    }
                    // Pushing the new chunk of data in target
                    write_and_check(target_sock,"PUSH ",5,error_buffer);
                    write_and_check(target_sock,target_file,strlen(target_file),error_buffer);
                    write_and_check(target_sock,"/",1,error_buffer);
                    write_and_check(target_sock,filename,strlen(filename),error_buffer);
                    write_and_check(target_sock," ",1,error_buffer);
                    number_to_string(number_buffer, snt);
                    if (atoi(number_buffer) != snt || snt <= 0) {
                        printf("NUMBER IS %s %d\n",number_buffer,snt);
                        exit(-1);
                    }
                    write_and_check(target_sock,number_buffer,strlen(number_buffer),error_buffer);
                    write_and_check(target_sock," ",1,error_buffer);
                    write_and_check(target_sock,buffer,snt,error_buffer);
                    bytes_pushed += snt;
                    data_sent -= snt;
                } while (data_sent > 0);

                // Pushing PUSH file -1 so the client knows we stopped sending
                // data
                write_and_check(target_sock,"PUSH ",5,error_buffer);
                write_and_check(target_sock,target_file,strlen(target_file),error_buffer);
                write_and_check(target_sock,"/",1,error_buffer);
                write_and_check(target_sock,filename,strlen(filename),error_buffer);
                write_and_check(target_sock," -1\n",4,error_buffer);
            }    
 
        }
        // Writing to logfile our results from the performed action. 

        // Creating SOURCE_DIR value (source_dir/sourcefile@hostname:port)
        char source_dir[1024];
        source_dir[0] = '\0';
        strcat(source_dir,source_file);
        strcat(source_dir,"/");
        strcat(source_dir,filename);
        strcat(source_dir,"@");
        strcat(source_dir,source_host);
        strcat(source_dir,":");
        number_to_string(number_buffer,source_port);
        strcat(source_dir,number_buffer);
        // Creating TARGET_DIR value (target_dir/targetfile@hostname:port)
        char target_dir[1024];
        target_dir[0] = '\0';
        strcat(target_dir,target_file);
        strcat(target_dir,"/");
        strcat(target_dir,filename);
        strcat(target_dir,"@");
        strcat(target_dir,target_host);
        strcat(target_dir,":");
        number_to_string(number_buffer,target_port);
        strcat(target_dir,number_buffer);

        pthread_mutex_lock(&log_mtx);
        // Writing to logfile
        if (strlen(error_buffer) > 0) {
            write_worker_result(logfile_fd,source_dir,target_dir,"PUSH","ERROR",error_buffer);
            write_worker_result(logfile_fd,source_dir,target_dir,"PULL","ERROR",error_buffer);
        }
        else {
            char details[1024];
            number_to_string(number_buffer,bytes_pushed);
            strcpy(details,number_buffer);
            strcat(details,"bytes pushed");
            write_worker_result(logfile_fd,source_dir,target_dir,"PUSH","SUCCESS",details);
            number_to_string(number_buffer,bytes_pulled);
            strcpy(details,number_buffer);
            strcat(details,"bytes pulled");
            write_worker_result(logfile_fd,source_dir,target_dir,"PULL","SUCCESS",details);
        }
       
        pthread_mutex_unlock(&log_mtx);
        close(source_sock);
        close(target_sock);

    }
    return NULL;
}

/* This function writes the given message using write, and if write fails, it 
 * appends an error message, for the reason of fail in error_buffer. 
 * This function will be used by worker threads, to reduce the number of 
 * conditional branches that check for fails of syscalls, as worker threads 
 * are using a lot of write syscalls */
void write_and_check(int fd,char* buf,int len,char* error_buffer) {
    if (write(fd,buf,len) < 0) {
        // We need to append an error message to the buffer
        strcat(error_buffer,"write: ");
        strcat(error_buffer,strerror(errno));
        strcat(error_buffer,",");
    }
}

void write_worker_result(int logfile_fd,char* source_dir,char* target_dir,char* operation,char* result,char* details) {
    // We use low-level I/O (write). The format is
    // [TIMESTAMP] [SOURCE_DIR] [TARGET_DIR] [THREAD_PID] [OPERATION] [RESULT] [DETAILS]
    char time_buffer[32],number_buffer[32];
    write(logfile_fd,"[",1);
    print_timestamp(time_buffer);
    write(logfile_fd,time_buffer,strlen(time_buffer));
    write(logfile_fd,"] [",3);
    // Writing SOURCE_DIR 
    write(logfile_fd,source_dir,strlen(source_dir));
    write(logfile_fd,"] [",3);
    // Writing TARGET_DIR
    write(logfile_fd,target_dir,strlen(target_dir));
    write(logfile_fd,"] [",3);
    // Writing THREAD_PID 
    int id = pthread_self();
    number_to_string(number_buffer,id);
    write(logfile_fd,number_buffer,strlen(number_buffer));
    write(logfile_fd,"] [",3);
    // Writing operation
    write(logfile_fd,operation,strlen(operation));
    write(logfile_fd,"] [",3);
    // Writing result
    write(logfile_fd,result,strlen(result));
    write(logfile_fd,"] [",3);
    // Writin details
    write(logfile_fd,details,strlen(details));
    write(logfile_fd,"]\n",2);
 
}

void place(pool_t* pool,char* action) {
    // Places a new action inside the buffer, when there is available space
    char bf[1024];
    strcpy(bf,action);
    pthread_mutex_lock(&buffer_mtx);
    while (pool->count >= buffer_size) {
        pthread_cond_wait(&cond_nonfull,&buffer_mtx);
    }

    pool->end = (pool->end + 1) % buffer_size;
    strcpy(pool->buffer[pool->end],bf);
    pool->count++;
    pthread_mutex_unlock(&buffer_mtx);
}

void obtain(pool_t* pool,char* action) {
    char* data;
    pthread_mutex_lock(&buffer_mtx);
    while (pool->count <= 0) {
        pthread_cond_wait(&cond_nonempty,&buffer_mtx);
    }
    data = pool->buffer[pool->start];
    pool->start = (pool->start + 1) % buffer_size;
    pool->count--;
    strcpy(action,data);
    pthread_mutex_unlock(&buffer_mtx);
}
