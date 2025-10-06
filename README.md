# network-file-sstem

This repo is a network file system program implemented as a project
for a Systems Programming course in my university. The pdf description of the 
assignment is also included in this github repo.

## Program Description

We are using 3 different programs to implement the network file system:

    - nfs_client
    - nfs_console
    - nfs_manager

These programs are using the network to communicate with each other. 

### nfs_client

nfs_client is connected with nfs_manager and implements the following actions
into local directories:

- LIST source_dir:  Sends to nfs_manager the files that are inside 
                    directory "source_dir"

-  PULL filename:   Sends the contents of the file "filename" to nfs_manager

- PUSH filename chunk_size data: Reads chunk_size bytes of data from nfs_manager
                                 and apends them to "filename". If chunk_size is
                                 0 or -1, nfs_client opens or closes the file.

nfs_client is a multi-threaded app, and each thread is created when we have a 
new connection, and it remains active until the file that it servers it's 
clossed (PUSH file -1).

The directory names should be given in the form /dir_name

#### Executing nfs_client

`./nfs_client -p <port_number>`

where port_number is the port we want nfs_client to use.

### nfs_console

nfs_console is the program that communicates with the user and nfs_manager. It 
reads commands from the user through stdin, and sends them to nfs_manager 
through a socket. After nfs_manager executes the given command, it sends it's 
output to nfs_console and nsf_console prints it to the user. 

#### Executing nfs_console

`./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>`

- <console-logfile>: A logfile that nfs_console writes all the given commands.
- <host_IP>: The hostname of nfs_manager
- <host_port>: The port nfs_manager uses for communicating with nfs_console

#### nfs_console Commands

- add <source> <target>: Adds a directory pair for synchronization.
- cancel <source>: Cancels the syncing of <source> and it's pair.
- shutdown: Shuts down nfs_manager and terminates.

### nfs_manager

nfs_manager, is the program that acts as a manager in the app. It's 
responsibilites are:

- Synchronizing directories between two different nfs_clients. 
- Reading commands from nfs_console and executing them.
- Reading directory pairs from a config file, and then synchronizing them

nfs_manager is a multi-threaded program, that uses worker threads, which are
subroutines that implementing the synchronization process between two 
directories.

### Executing nfs_manager

`./nfs_manager -l <manager_logfile> -c <config_file> -n <worker_limit>
-p <port_number> -b <bufferSize>`

- <manager_logfile>: nfs_manager's logfile
- <config_file>: A config_file that contains pairs, that need to be synced 
at the start of the program
- <worker_limit>: The number of workers. Maximum number of threads used are 
worker_limit + 1 (nfs_manager main program also uses 1 thread).
- <port_number>: The port that nfs_manager uses to communicate with nfs_console

- <buferSize>: Our workers are fetching pair's of directories in a concurrent
type of way, similar to the round table problem, using a buffer. So the 
buferSize is the number of pair's that can be available at the same time, for 
a worker to fetch.

## Compilation

To compile each of the three programs indivintually, we can use the command
`make <program_name>` (e.g. `make nfs_client`), or we can compile all programs
using 'make all'

Also we can use `make clean` in case we want to delete all our executables
and object files.

## Notes

This was implemented as a university project, so it was required to use a lot 
of syscalls in some areas. So you may come across a whole bunch of write calls 
instead of a printf.

This project was built using git controll, but it was in a private repository
(in github classroom) and i couldn't publish it with it's previous git history.
So i republish it here as a public repository

Every directory should be given in the form <source_dir>@<hostname>:<port>
