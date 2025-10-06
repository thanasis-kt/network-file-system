/** Source file for lnode structure. lnode is a linked-list that will be used 
 * by sync_info_mem_store for seperate chaining */ 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../include/lnode.h"

// Creates  lnode and returns a pointer to it
lnode* lnode_create(dir_info value,lnode* next) {
    lnode* node = malloc(sizeof(lnode));
    if (node == NULL) {
        perror("ERROR! malloc failed\n");
        exit(-1);
    }
    node->value = value;
    node->next = next;
    return node;
}

// Setters
void lnode_set_value(lnode* node,dir_info new_value) {
    node->value = new_value;
}

void lnode_set_next(lnode* node, lnode* next) {
    node->next = next;
}


// Getters
dir_info lnode_get_value(lnode* node) {
    return node->value;
}

lnode* lnode_get_next(lnode* node) {
    return node->next;
}

// Adds a node next to head
void lnode_add_next(lnode* node,dir_info new_value) {
   lnode* new_node = lnode_create(new_value,node->next); 
   node->next = new_node;
}

// Removes node with given value
void lnode_remove(lnode** node,char* source) {
    while (*node != NULL) {
        if (!strcmp((*node)->value.key,source)) {
            lnode* temp = *node;
            *node = (*node)->next;
            free(temp);
            return;
        }
        // We point to the next node
        node = &((*node)->next);
    }
}

// Removes head node
void lnode_remove_head(lnode **node) {
    lnode* temp = *node;
    *node = (*node)->next;
    free(temp);
}

// Returns the directory information of source_dir or NULL if not found
dir_info* lnode_find(lnode *node, char *source) {
    while (node != NULL) {
        // source_dir is our key
        if (!strcmp(node->value.key,source)) {
            return &(node->value);
        }
        node = node->next;
    }
    return NULL;
}

// Frees lnode from memory
void lnode_destroy(lnode** node) {
    while (*node != NULL) {
        lnode* temp = *node;
        *node = (*node)->next;
        free(temp);
    }
}
