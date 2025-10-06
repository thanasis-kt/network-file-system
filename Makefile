# Our directories
INCLUDE = include
SOURCE = src

#Compile Options
CFLAGS =  -g -I $(INCLUDE) -Wall


# Files to be compiled (files without main)
OBJS = $(SOURCE)/lnode.o $(SOURCE)/map.o $(SOURCE)/nfs.o 

# Our executable names
EXEC_MANAGER = nfs_manager

EXEC_CONSOLE = nfs_console

EXEC_CLIENT= nfs_client

# To compile all files
all: $(EXEC_MANAGER)  $(EXEC_CLIENT) $(EXEC_CONSOLE)

$(EXEC_CLIENT): $(OBJS) $(SOURCE)/nfs_client.o
	gcc $(OBJS) $(SOURCE)/nfs_client.o -o $(EXEC_CLIENT) $(FLAGS)

$(EXEC_MANAGER): $(OBJS) $(SOURCE)/nfs_manager.o
	gcc $(OBJS) $(SOURCE)/nfs_manager.o -o $(EXEC_MANAGER) $(FLAGS)

$(EXEC_CONSOLE): $(OBJS) $(SOURCE)/nfs_console.o
	gcc $(OBJS)  $(SOURCE)/nfs_console.o -o $(EXEC_CONSOLE) $(FLAGS)


# Deletes all files created by makefile
clean: 
	rm -f $(OBJS) $(EXEC_MANAGER) $(EXEC_CONSOLE) $(EXEC_WORKER) $(SOURCE)/nfs_console.o $(SOURCE)/nfs_manager.o $(SOURCE)/nfs_client.o

