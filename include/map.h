/* Header file for sync_info_mem_store data structure. This structure is used 
 * by nfs_manager program, to allow fast access (O(1) amortised) to the 
 * information stored for every pair of files.
 * The sync_info_mem data structure is implemented using a hash table, with 
 * linked chains, using theory princibles from Data Structures course, to 
 * improve efficency (rehashing,using prime sizes etc). We use source directory as a primary key
 * */
#include <stdio.h>
#include <stdlib.h>
#include "lnode.h"
#include "nfs.h"
#define INITIAL_SIZE 193 // initial size for our hash_table

#pragma once

typedef struct {
    // Our hash table uses seperate chaining
    lnode** hash_table; 
    int size;
    int capacity;
} map;

// Creates and initializes a map data structure
map* map_create(void);

// Adds new_value into structure. source and target are entries of
//  <source_dir>@<ip>:<port>
void map_add(map* mem,char* source,char* target);

// Removes value with given source_dir
void map_remove(map* mem,char* source);

// Returns status of the source dir or NULL if it doesn't exist
dir_info* map_find(map* mem,char* source);

// Frees the structure from the memory 
void map_destroy(map* mem);

