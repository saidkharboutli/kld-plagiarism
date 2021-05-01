/**
 * File:        plagiarism.c
 * Author:      Mohammad S Kharboutli
 * Date:        29 NOV 2020
 * Course:      CS214: Systems Programming
 * University:  Rutgers--New Brunswick
 * 
 * Summary of File: Uses multithreading to search the current directory
 * for folders and files, specifically comparing each file against every
 * other file for possible plaigerism using Kullback–Leibler divergence.
 */

#include "plagiarism.h"

/**
 * void* file_handler(void* args)
 * 
 * Description:
 *     When directory_handler finds a file, it will call this function to
 *     go through and parse each found file.
 * 
 * Parameters:
 *     void* args - arguments that will take form as struct args.
 * 
 * Returns:
 *      void* NULL
 */
void* file_handler(void* args) {
    struct args arguments = {((struct args*)args)->shared_locker, ((struct args*)args)->head_file, ((struct args*)args)->path};
    FILE* curr_file = fopen(arguments.path, "r");
    if(curr_file == NULL) {
        printf("ERROR: FILE INACCESSIBLE\n");
        return NULL;
    }
    fseek(curr_file, 0L, SEEK_END); off_t size = ftell(curr_file); rewind(curr_file);
    char* buffer = malloc((int)size); fread(buffer, 1, size, curr_file);
    char *curr_str = malloc((int)size);
    struct file_node *new_file = malloc(sizeof(struct file_node));
    fclose(curr_file);
    new_file->total_tokens = 0;
    new_file->path = arguments.path;
    new_file->head_token = NULL;
    curr_str[0] = '\0';
    int curr_index = 0;
    int total_token_count = 0;
    int found = 0;
    for(int i = 0; i <= size; i++) {
        found = 0;
        char curr_char = (char) buffer[i];
        if(isspace(curr_char) || curr_char == '\0') {
            if(curr_str[0] == '\0') {
                continue;
            }
            for(struct token_node *curr_token = new_file->head_token; curr_token != NULL; curr_token = curr_token->next_node) {
                if(strcmp(curr_token->token, curr_str) == 0) {
                    curr_token->count = curr_token->count + 1;
                    total_token_count++;
                    found = 1;
                    break;
                }
            }
            if(found == 0) {
                struct token_node *new_token = malloc(sizeof(struct token_node));
                new_token->count = 1;
                total_token_count++;
                new_token->token = malloc(sizeof(curr_str));
                strcpy(new_token->token, curr_str);
                if(new_file->head_token == NULL || strcmp(new_file->head_token->token, new_token->token) >= 0) {
                    new_token->next_node = new_file->head_token;
                    new_file->head_token = new_token;
                } else {
                    struct token_node *curr_token = new_file->head_token; 
                    while (curr_token->next_node != NULL && strcmp(curr_token->next_node->token, new_token->token) < 0) { 
                        curr_token = curr_token->next_node; 
                    }
                    new_token->next_node = curr_token->next_node; 
                    curr_token->next_node = new_token;
                }
            }
            curr_str[0] = '\0';
            curr_index = 0;
            continue;
        }
        if(isalnum(curr_char) || curr_char == '-') {
            curr_str[curr_index] = tolower(curr_char);
            curr_index++;
            curr_str[curr_index] = '\0';
            continue;
        }
    }
    new_file->total_tokens = total_token_count;
    pthread_mutex_lock(arguments.shared_locker);
    if(arguments.head_file->next_file == NULL || arguments.head_file->next_file->total_tokens >= new_file->total_tokens) {
        new_file->next_file = arguments.head_file->next_file;
        arguments.head_file->next_file = new_file;
    } else {
        struct file_node *curr_file = arguments.head_file->next_file;
        while (curr_file->next_file != NULL && curr_file->next_file->total_tokens < new_file->total_tokens) { 
            curr_file = curr_file->next_file; 
        } 
        new_file->next_file = curr_file->next_file;
        curr_file->next_file = new_file;
    }
    pthread_mutex_unlock(arguments.shared_locker);
    free(buffer);
    free(curr_str);
    return NULL;
}

/**
 * void* directory_handler(void* args)
 * 
 * Description:
 *     A threaded directory searcher--it will find directories and call new directory
 *     handlers or file handlers to search deeper within the directories.
 * 
 * Parameters:
 *     void* args - arguments that will take form as struct args.
 * 
 * Returns:
 *     void* NULL
 */
void* directory_handler(void* args) {
    char* base_path = ((struct args*)args)->path;
    DIR* curr_dir = opendir(base_path);    /*sanitize the input path*/
    if(curr_dir) {
        struct dirent *subdir = NULL;
        struct pthread_tracker *head = NULL;
        while((subdir=readdir(curr_dir)) != NULL) {
            if(strcmp(subdir->d_name, ".") == 0 || strcmp(subdir->d_name, "..") == 0) {
                continue;
            }
            if(subdir->d_type == DT_DIR) {
                struct pthread_tracker *new_tracker = malloc(sizeof(struct pthread_tracker));
                struct args *arguments = malloc(sizeof(struct args));
                new_tracker->arguments = arguments;
                arguments->path = malloc(strlen(((struct args*)args)->path) + strlen(subdir->d_name) + 2);
                arguments->head_file = ((struct args*)args)->head_file;
                arguments->shared_locker = ((struct args*)args)->shared_locker;
                strcpy(arguments->path, base_path);
                arguments->path = strcat(arguments->path, "/");
                arguments->path = strcat(arguments->path, subdir->d_name);
                pthread_create(&(new_tracker->curr_worker), NULL, directory_handler, arguments);
                new_tracker->next_tracker = head;
                head = new_tracker;
            } else if(DT_REG) {
                struct pthread_tracker *new_tracker = malloc(sizeof(struct pthread_tracker));
                struct args *arguments = malloc(sizeof(struct args));
                new_tracker->arguments = arguments;
                arguments->path = malloc(strlen(((struct args*)args)->path) + strlen(subdir->d_name) + 2);
                arguments->head_file = ((struct args*)args)->head_file;
                arguments->shared_locker = ((struct args*)args)->shared_locker;
                strcpy(arguments->path, base_path);
                arguments->path = strcat(arguments->path, "/");
                arguments->path = strcat(arguments->path, subdir->d_name);
                pthread_create(&(new_tracker->curr_worker), NULL, file_handler, arguments);
                new_tracker->next_tracker = head;
                head = new_tracker;
            }
       }
       closedir(curr_dir);
       struct pthread_tracker *curr = head;
       while(curr != NULL) {
           struct pthread_tracker *next = curr->next_tracker;
           pthread_join(curr->curr_worker, NULL);
           free(curr->arguments);
           free(curr);
           curr = next;
        }
        return NULL;
   } else {
       /*EXPAND ERROR HANDLING*/
       printf("ERROR: DIRECTORY NOT ACCESSIBLE\n");
       return NULL;
   }

}


/**
 * void* directory_handler(void* args)
 * 
 * Description:
 *     Will go through a given command line directory and search for
 *     files to parse. It is threaded search. Then it will take the parsed
 *     tokens and calculate the JSD to see possible plagerism.
 * 
 * Parameters:
 *     int argc - the number of command line arguments
 *     char** argv - the list of command line arguments
 * 
 * Returns:
 *      int - exit code
 */
int main(int argc, char** argv) {
    char* input_directory = argv[1];
    if(argv[1] == NULL) {
        printf("ERROR: NO INPUT DIRECTORY\n");
        return 1;
    }
    DIR* init_dir = opendir(input_directory);
    if(init_dir) {
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        struct file_node *head_file = &(struct file_node) {-1, NULL, NULL, NULL};
        struct args init_args = {&mutex, head_file, input_directory};
        directory_handler((void*)&init_args);
        
        if(head_file->next_file == NULL || head_file->next_file->next_file == NULL) {
            printf("ERROR: NOT ENOUGH FILES\n");
            return 1;
        }
        for(struct file_node *curr = head_file->next_file; curr != NULL; curr = curr->next_file) {
            for(struct token_node *curr_tok = curr->head_token; curr_tok != NULL; curr_tok = curr_tok->next_node) {
                curr_tok->count = curr_tok->count / ((double)curr->total_tokens);
            }
        }
        for(struct file_node *first = head_file->next_file; first != NULL; first = first->next_file) {
            for(struct file_node *sec = first->next_file; sec != NULL; sec = sec->next_file) {
                double kld = 0;
                double kld1 = 0, kld2 = 0;
                for(struct token_node *first_tok = first->head_token, *sec_tok = sec->head_token; first_tok != NULL || sec_tok != NULL;) {
                    double first_dist = 0, second_dist = 0;
                    if((sec_tok == NULL && first_tok != NULL) || (sec_tok != NULL && strcmp(first_tok->token, sec_tok->token)) < 0) {
                        first_dist = first_tok->count;
                        kld1 += first_dist * log10(first_dist / (first_dist / 2.0));
                        first_tok = first_tok->next_node;
                    } else if((first_tok == NULL && sec_tok != NULL) || (first_tok != NULL && strcmp(first_tok->token, sec_tok->token)) > 0) {
                        second_dist = sec_tok->count;
                        kld2 += second_dist * log10(second_dist / (second_dist / 2.0));
                        sec_tok = sec_tok->next_node;
                    } else {
                        first_dist = first_tok->count;
                        second_dist = sec_tok->count;
                        first_tok = first_tok->next_node; sec_tok = sec_tok->next_node;
                        kld1 += first_dist * log10(first_dist / ((first_dist + second_dist) / 2.0));
                        kld2 += second_dist * log10(second_dist / ((first_dist + second_dist) / 2.0));
                    }
                }
                kld = (kld1 + kld2) / 2.0;
                printf("\e[0m");
                if(kld >= 0 && kld <=0.1) {
                    printf("\e[0;31m");
                } else if(kld <= 0.15) {
                    printf("\e[0;33m");
                } else if(kld <= 0.2) {
                    printf("\e[0;32m");
                } else if(kld <= 0.25) {
                    printf("\e[0;36m");
                } else if(kld <= 0.3) {
                    printf("\e[0;34m");
                } else if(kld > 0.3) {
                    printf("\e[0;37m");
                } else {
                    printf("ERROR: CALCULATED NEGATIVE JSD\n");
                }
                printf("%f ", kld);
                printf("\e[0m");
                printf("%s and %s\n", first->path, sec->path);
            }
        }
        struct file_node *curr_file = head_file->next_file;
        while(curr_file != NULL) {
            struct token_node *curr_tok = curr_file->head_token;
            while(curr_tok != NULL) {
                struct token_node *next = curr_tok->next_node;
                free(curr_tok->token);
                free(curr_tok);
                curr_tok = next;
            }
            struct file_node *next = curr_file->next_file;
            free(curr_file->path);
            free(curr_file);
            curr_file = next;
        }
        return 0;
    } else {
        printf("ERROR: INACESSABLE DIRECTORY\n");
        return 1;
    }
}
