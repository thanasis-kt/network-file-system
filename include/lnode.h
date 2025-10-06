/** Header file for lnode.c. lnode is a linked-list structure
 * that contains dir_info values and it is used by sync_info_mem_store 
 * iternally 
 **/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#pragma once

typedef struct linked_node lnode;

typedef struct {
    //TODO comments
    char key[256]; // Our key is the <source_dir>@<ip>:<port> format

    char source_dir[128]; // Our source directory
    char source_host[128]; 
    int source_port;      
    char target_dir[128]; // Our target directory
    char target_host[128];
    int target_port;
    bool is_active; // Is true if the directory is actively monitored 
} dir_info;

struct linked_node {
    dir_info value;
    lnode* next;
};

// Creates a lnode and returns a pointer to it
lnode* lnode_create(dir_info value,lnode* next);

// Setters
void lnode_set_value(lnode* node,dir_info new_value);
void lnode_set_next(lnode* node,lnode* next);

// Removes head node
void lnode_remove_head(lnode **node);

// Getters
dir_info lnode_get_value(lnode* node);
lnode* lnode_get_next(lnode* node);

// Adds a node next to head
void lnode_add_next(lnode* node,dir_info new_value);

// Removes node with given value
void lnode_remove(lnode** node,char* source);

// Returns the directory information of source == key or NULL if not found
dir_info* lnode_find(lnode* node,char* source);

// Frees lnode from memory
void lnode_destroy(lnode** node);

