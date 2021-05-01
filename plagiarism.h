#ifndef PLAGIARISM_H
#define PLAGIARISM_H
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

struct args {
    pthread_mutex_t *shared_locker;
    struct file_node *head_file;
    char* path;
};

struct file_node {
    int total_tokens;
    char *path;
    struct file_node *next_file;
    struct token_node *head_token;
};

struct token_node {
    double count;
    char *token;
    struct token_node *next_node;
};


struct pthread_tracker {
    pthread_t curr_worker;
    struct pthread_tracker *next_tracker;
    struct args *arguments;
};

#endif