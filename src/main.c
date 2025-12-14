#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "file_utils.h"
#include "lists.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))


int finish_work_flag = 0;
/*


    ADD


*/
// finding whether a path is a subpath of another path

int validate_target(node_sc* to_add, char* tok)
{
    // If an element is already a target or a source of another element in the list, dont add it to the targets and
    // print an error message.
    if (find_element_by_target_help(&to_add->targets, tok))
    {
        printf("This target already exists(%s)!\n", tok);
        return 0;
    }
    if (is_target_in_source(to_add->source_friendly, tok))
    {
        printf("Target cannot lie in source!\n");
        return 0;
    }
    if (find_element_by_target(tok))
    {
        printf("Target already used!\n");
        return 0;
    }
    return 1;
}
int parse_targets(node_sc* to_add)
{
    char* tok = strtok(NULL, " ");
    int cnt = 0;
    while (tok != NULL)
    {
        // If the directory is not empty, write and continue parsing, is_empty_dir ensures that it is empty or creates
        // it if it doesnt exist
        if (is_empty_dir(tok) < 0)
        {
            printf("Target has to be an empty directory!\n");
            tok = strtok(NULL, " ");
            continue;
        }

        char* real_tok = realpath(tok, NULL);
        if (real_tok == NULL)
        {
            fprintf(stderr, "realpath failed for '%s': %s\n", tok, strerror(errno));
            ERR("realpath");
        }
        // Checks whether the target isnt conflicting
        if (validate_target(to_add, tok) == 0)
        {
            free(real_tok);
            tok = strtok(NULL, " ");
            continue;
        }

        // Create new target entry
        node_tr* new_node = malloc(sizeof(node_tr));

        if (new_node == NULL)
        {
            ERR("malloc failed");
        }

        new_node->target_friendly = strdup(tok);
        new_node->target_full = strdup(real_tok);

        if (new_node->target_friendly == NULL || new_node->target_full == NULL)
        {
            ERR("strdup failed");
        }

        new_node->next = NULL;
        new_node->previous = NULL;

        // Add to the list
        list_target_add(&to_add->targets, new_node);
        init_backup_add_job(strdup(to_add->source_full), strdup(new_node->target_full));
        
        printf("Full source: '%s', friendly: '%s'\n", to_add->source_full, to_add->source_friendly);
        cnt++;
        tok = strtok(NULL, " ");
        free(real_tok);
    }
    puts("Finished parsing targets");
    return cnt;
}

int take_input()
{
    char* tok = strtok(NULL, " ");
    if (tok == NULL)
        return 0;

    // Setting variables
    int was_source_added = 0;
    char* source_full = realpath(tok, NULL);
    if (source_full == NULL)
    {
        fprintf(stderr, "Source directory doesn't exist '%s': %s\n", tok, strerror(errno));
        return 0;
    }
    if (find_element_by_target(tok) == 1)
    {
        printf("Source is a target of a different backup operation.\n");
        return 0;
    }

    // Checking whether we have an element with this source already active
    node_sc* source_elem = find_element_by_source(tok);

    // If not create it
    if (source_elem == NULL)
    {
        source_elem = malloc(sizeof(struct Node_source));
        if (source_elem == NULL)
        {
            ERR("malloc failed");
        }

        source_elem->source_friendly = strdup(tok);
        source_elem->source_full = strdup(source_full);

        if (source_elem->source_friendly == NULL || source_elem->source_full == NULL)
        {
            ERR("strdup failed");
        }

        source_elem->targets.head = NULL;
        source_elem->targets.tail = NULL;
        source_elem->targets.size = 0;
        was_source_added = 1;
        fprintf(stderr, "NEW node_sc %p, source='%s'\n", (void*)source_elem, source_elem->source_full);
    }

    // Parsing the list of targets
    int cnt = parse_targets(source_elem);

    // If we have added something, and we didn't have the element before add the new node to the list of active
    // whatchers
    if (cnt > 0 && was_source_added)
    {
        list_source_add(source_elem);
    }
    // If the lenght of list of our target is zero delete it
    else if (source_elem->targets.size == 0)
    {
        delete_source_node(source_elem);
    }

    free(source_full);
    puts("Finished taking input");
    return 1;
}

/*


LIST


*/
void print_targets(list_tg* l)
{
    printf("amount of targets: %d\n", l->size);

    node_tr* current = l->head;
    while (current != NULL)
    {
        printf("    Target: '%s'  real path: '%s' \n", current->target_friendly, current->target_full);
        current = current->next;
    }
}
void list_sources_and_targets()
{
    list_sc* l = &backups;
    node_sc* current = l->head;
    while (current != NULL)
    {
        printf("Source: '%s', real path: '%s' ,", current->source_friendly, current->source_full);
        print_targets(&current->targets);
        current = current->next;
    }
}

/*


EXIT


*/

void delete_backups_list()
{
    // Free all elements in the list
    node_sc* current = backups.head;
    while (current != NULL)
    {
        node_sc* temp = current;
        current = current->next;
        delete_source_node(temp);
    }

    // Reset list pointers
    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;
}

/*


END



*/
void end(char* source_friendly)
{
    char* tok = strtok(NULL, " ");

    // Find the source node
    node_sc* node_found = find_element_by_source(source_friendly);
    if (node_found == NULL)
    {
        puts("No such source");
        return;
    }

    // For every target try to delete it from the list of targets
    int cnt = 0;
    while (tok != NULL)
    {
        fprintf(stderr, "Trying to delete %s \n", tok);
        list_target_delete(&node_found->targets, tok);
        cnt++;
        tok = strtok(NULL, " ");
    }

    if (cnt == 0)
    {
        puts("Nothing to delete");
    }

    // If we have deleted all targets from the source node then we can remove it
    if (node_found->targets.size == 0)
    {
        list_source_delete(source_friendly);
    }
}

void* backup_handler(void* arg)
{
    while (1)
    {
        while (init_backup_tasks.size != 0)
        {
            initial_backup(init_backup_tasks.head->source_full, init_backup_tasks.head->target_full);
            init_backup_job_done();
        }

    }

    return 1;
}
void* input_handler(void* arg)
{
    size_t z = 0;
    char* buff = NULL;
    int n = 0;
    int k;
    int thread_created = 0;
    pthread_t backup_thread = 0;
    while ((n = getline(&buff, &z, stdin)) > 1)
    {
        if (buff[n - 1] == '\n')
        {
            buff[n - 1] = '\0';
        }

        char* tok = strtok(buff, " ");
        if (tok == NULL)
        {
            printf("Invalid syntax\n");
            continue;
        }
        // input in the form add <source path> <target path> with multiple target paths
        if (strcmp(tok, "add") == 0)
        {
            if (thread_created == 0)
            {
                if (pthread_create(&backup_thread, NULL, backup_handler, NULL) == 0)
                {
                    ERR("pthread_create backup");
                }
            }
            if ((k = take_input()) == 0)
            {
                // do smth
                puts("Invalid syntax\n");
                continue;
            }
        }
        // input in the form end <source path> <target path> with multiple target paths
        else if (strcmp(tok, "end") == 0)
        {
            tok = strtok(NULL, " ");
            if (tok == NULL)
            {
                puts("Invalid syntax\n");
                continue;
            }

            end(tok);
        }
        else if (strcmp(tok, "restore") == 0)
        {
        }
        else if (strcmp(tok, "exit") == 0)
        {
            finish_work_flag = 1;

            if(thread_created){
                pthread_join(backup_thread, NULL);
            }
            
            delete_backups_list();
            free(buff);
            return 0;
        }
        else if (strcmp(tok, "list") == 0)
        {
            list_sources_and_targets();
        }
    }
    if(thread_created){
        pthread_join(backup_thread, NULL);
    }
    delete_backups_list();
    free(buff);
    return 1;
}
int main()
{
    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;
    wd_list.head = NULL;
    wd_list.tail = NULL;
    wd_list.size = 0;

    if ((fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC)) == -1)
    {
        ERR("inotify_init1");
    }
    pthread_t input_thread;
    if (pthread_create(&input_thread, NULL, input_handler, NULL) != 0)
    {
        ERR("pthread_create");
    }

    pthread_join(input_thread, NULL);
    close(fd);
    return 0;
}
