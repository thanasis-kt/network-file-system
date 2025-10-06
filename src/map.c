/* sync_info_mem_store is a hash_table implemented using seperate chaining. It 
 * stores dir_info values and uses target_dir as primary key.
 * */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/map.h"
#include "../include/lnode.h"
#include "../include/nfs.h"

#define LOAD_FACTOR 0.9 // when  size / capacity > load factor, we rehash

// According to data structure theory, we improve our hash table efficiency by
// selecting a prime integer as array's size. Also to accieve a O(1) complexity in
// rehashing we use at least double the size. So this array helps to accieve 
// those goals
int PRIME_SIZES[] = {53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241,
	786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};


// This function returns a hash code for the string value
uint hash_code(char* value);

// This function will be used to rehash our hash_table for efficiently
void map_rehash(map* mem);

// Creates and initializes a map data structure
map* map_create(void) {
    map* mem = malloc(sizeof(map));
    if (mem == NULL) {
        perror("ERRROR! malloc failed\n");
        exit(-1);
    }
    mem->size = 0;
    mem->capacity = INITIAL_SIZE;
    mem->hash_table = malloc(sizeof(lnode*) * mem->capacity);
    if (mem->hash_table == NULL) {
        perror("ERRROR! malloc failed\n");
        exit(-1);
    }
    // We initialize with NULL values
    for (int i = 0; i < mem->capacity; i++) {
        mem->hash_table[i] = NULL;
    }
    return mem;
}

// Adds new_value into structure, or if it exists it modifies it
void map_add(map* mem,char* source,char* target) {
    dir_info new_value;
    strcpy(new_value.key,source);
    decode_format(source,new_value.source_dir,new_value.source_host,&new_value.source_port);
    decode_format(target,new_value.target_dir,new_value.target_host,&new_value.target_port);
    new_value.is_active = true;
    // If it already exists we just replace value
    dir_info* pair = map_find(mem,new_value.key);
    if (pair != NULL) {
        *pair = new_value;
    }
    else {
        // New value needs to be saved in chain hash_table[pos]
        int pos = hash_code(new_value.key) % mem->capacity;
        // We either create the chain or add it to the chain
        if (mem->hash_table[pos] == NULL) {
            mem->hash_table[pos] = lnode_create(new_value, NULL);
        }
        else {
            lnode_add_next(mem->hash_table[pos],new_value);
        }
        mem->size++;
        // Checking for rehash, if necessary
        if ((mem->size / (double)mem->capacity) > LOAD_FACTOR) {
            map_rehash(mem);
        }
    }    
}


// Removes value with given source
void map_remove(map* mem,char* source) {
    int pos = hash_code(source) % mem->capacity;
    lnode_remove(&(mem->hash_table[pos]),source);
}

// Returns status of the source or NULL if it doesn't exist
dir_info* map_find(map* mem,char* source) {
    int pos = hash_code(source) % mem->capacity;
    return lnode_find(mem->hash_table[pos], source);
}

// Frees the structure from the memory 
void map_destroy(map* mem) {
    for (int i = 0; i < mem->capacity; i++) {
        // We destroy all the chains we have created
        if (mem->hash_table[i] != NULL) {
            lnode_destroy(&(mem->hash_table[i]));
        }
    }
    free(mem->hash_table);
    free(mem);
}

void map_rehash(map* mem) {
    // Selecting a new capacity from primes array 
    int i;
    i = 0;
    int old_capacity = mem->capacity;
    // FIX double when PRIME_SIZES ends
    int new_capacity = mem->capacity;
    while (mem->capacity >= PRIME_SIZES[i]) {
        if (PRIME_SIZES[i] == 1610612741) {
            new_capacity = 2 * mem->capacity;
            break;
        }        
        new_capacity = PRIME_SIZES[i];
        i++;
    }
    mem->capacity = new_capacity;
    lnode** old_hash_table = mem->hash_table;
    mem->size = 0;
    mem->hash_table = malloc(sizeof(lnode*) * mem->capacity);
    // We put all the values from the old chains to our new hash table
    //  and then we free the space we allocated for old hash_table
    for (int i = 0; i < old_capacity; i++) {
        while (old_hash_table[i] != NULL) {
            dir_info dir = lnode_get_value(old_hash_table[i]);
            char target[1024];
            target[0] = '\0';
            strcat(target,dir.target_dir);
            strcat(target,"@");
            strcat(target,dir.target_host);
            strcat(target,":");
            char num[32];
            strcat(target,number_to_string(num,dir.target_port));
            map_add(mem,dir.key,target);
            lnode_remove_head(&(old_hash_table[i]));
        }
    }
    free(old_hash_table);
}

// We use the dbj2 hash function
uint hash_code(char* value) {
    uint hash = 5381;
    int i = 0;
    while (value[i] != '\0') {
        hash = (hash * 33) + value[i++];
    }
    return i;
}
