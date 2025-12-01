#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))



typedef struct Node_target
{
    char* target_friendly;
    char* target_full;
    struct Node_target* next;
    struct Node_target* previous;
} node_tr;

typedef struct List_target
{
    struct Node_target* head;
    struct Node_target* tail;
    int size;
} list_tg;

typedef struct Node_source
{
    char* source_full;
    char* source_friendly;
    struct List_target targets;
    struct Node_source* next;
    struct Node_source* previous;
} node_sc;

typedef struct List_source
{
    struct Node_source* head;
    struct Node_source* tail;
    int size;
} list_sc;

/* GLOBAL VARIABLES*/
list_sc backups;
int inotify_fd;
/*-------------------*/

/* 

LIST FUNCTIONS

*/

// Helper function to delete a single target node
void delete_target_node(node_tr* node)
{
    if (node != NULL)
    {
        free(node->target_friendly);
        free(node->target_full);
        free(node);
    }
}

// Helper function to delete a single source node
void delete_source_node(node_sc* node)
{
    if (node != NULL)
    {
        free(node->source_friendly);
        free(node->source_full);
      
        node_tr* current_target = node->targets.head;
        while (current_target != NULL)
        {
            node_tr* temp = current_target;
            current_target = current_target->next;
            delete_target_node(temp);
        }
        free(node);
    }
}

// Function to add a target node at the beginning of target list
void list_target_add(list_tg* l, node_tr* new_node)
{   
    if (l == NULL || new_node == NULL)
    {
        return;
    }

    new_node->next = l->head;
    new_node->previous = NULL;

    if (l->head != NULL)
    {
        l->head->previous = new_node;
    }
    else
    {
        l->tail = new_node;
    }

    l->head = new_node;
    l->size++;
}

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return;
    }

    node_tr* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->target_friendly, target) == 0)
        {
            // Found the node to delete
            if (current->previous != NULL)
            {
                current->previous->next = current->next;
            }
            else
            {
                l->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->previous = current->previous;
            }
            else
            {
                l->tail = current->previous;
            }

            delete_target_node(current);
            printf("Deleted node with full path: %s\n", current->target_full);
            l->size--;
            return;
        }
        current = current->next;
    }
    printf("Target not found: %s\n", target);
}

// Function to add a source node at the beginning of source list
void list_source_add( node_sc* new_node)
{   
    list_sc* l = &backups;
    if (l == NULL || new_node == NULL)
    {
        return;
    }
    fprintf(stderr, "ADD node_sc %p (source=%s)\n", (void*)new_node, new_node->source_friendly);

    new_node->next = l->head;
    new_node->previous = NULL;

    if (l->head != NULL)
    {
        l->head->previous = new_node;
    }
    else
    {
        l->tail = new_node;
    }

    l->head = new_node;
    l->size++;
}

// Function to delete a source node by source name
void list_source_delete( char* source)
{   
    list_sc* l = &backups;
    if (l == NULL || source == NULL)
    {
        return;
    }

    node_sc* current = l->head;
    fprintf(stderr, "DEL node_sc %p (source=%s)\n", (void*)current, current->source_friendly);

    while (current != NULL)
    {
        if (strcmp(current->source_friendly, source) == 0)
        {
            
            if (current->previous != NULL)
            {
                current->previous->next = current->next;
            }
            else
            {
                l->head = current->next;  
            }

            if (current->next != NULL)
            {
                current->next->previous = current->previous;
            }
            else
            {
                l->tail = current->previous; 
            }

            delete_source_node(current);
            l->size--;
            return;
        }
        current = current->next;
    }
}

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return 0;
    }

    node_tr* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->target_friendly, target) == 0)
        {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char* target)
{
    list_sc* l = &backups;

    if (l == NULL || target == NULL)
    {
        return 0;
    }
    node_sc* current = l->head;
    while (current != NULL)
    {
        if (find_element_by_target_help(&current->targets, target) == 1)
        {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Function to find element by friendly source name
node_sc* find_element_by_source( char* source)
{   list_sc *l = &backups;
    if (l == NULL || source == NULL)
    {
        return NULL;
    }

    node_sc* current = l->head;
    while (current != NULL)
    {
        if (current->source_friendly == NULL)
        {
            fprintf(stderr, "BUG: current=%p, current->source == NULL (list corrupted)\n", (void*)current);
            abort();
        }
        if (strcmp(current->source_friendly, source) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


/*--------------------------------------------------*/
/*
    HELPER FUNCTIONS
*/
void checked_mkdir(char* path)
{
    if (mkdir(path, 0755) != 0)
    {
        if (errno != EEXIST)
            ERR("mkdir");
    }
    struct stat s;
    if (stat(path, &s))
        ERR("stat");
    if (!S_ISDIR(s.st_mode))
    {
        printf("%s is not a valid dir, exiting\n", path);
        exit(EXIT_FAILURE);
    }
}


void make_path(char* path)
{
    char* temp = strdup(path);
    if (!temp)
        ERR("strdup");
    for (char* p = temp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            checked_mkdir(temp);
            *p = '/';
        }
    }
    free(temp);
}

/*checking whether the directory exists and is empty. if it doesnt exist it creates it.
    return -1 when is not a directory
    return 0 when it is not empty or didnt exist
*/
int is_empty_dir(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        make_path(path);
        return 0;
    }

    if (!S_ISDIR(st.st_mode))
    {
        return -1;
    }

    DIR* d = opendir(path);
    if (!d)
    {
        return -1;
    }

    struct dirent* ent;
    int cnt = 0;
    while ((ent = readdir(d)) != NULL)
    {
        cnt++;
    }
    closedir(d);

    if (cnt == 2)
    {
        return 0;
    }

    return -1;
}

int min(int a, int b) { return a < b ? a : b; }

/*-----------------------------------------------------*/



/*

BACKUPS

*/

int nftw_walk() { return 1; }

void initial_backup(char* source, char* target) {



}

/*


ADD


*/
// finding whether a path is a subpath of another path
int is_target_in_source(char* source, char* target)
{
    int i = 0;
    int n = min((int)strlen(target), (int)strlen(source));
    while (source[i] == target[i] && i < n)
    {
        i++;
    }
    if (i == (int)strlen(source))
    {
        return 1;
    }

    return 0;
}
int parse_targets(node_sc* to_add)
{
    char* tok = strtok(NULL, " ");
    int cnt = 0;
    while (tok != NULL)
    {
        //If the directory is not empty, write and continue parsing, is_empty_dir ensures that it is empty or creates it if it doesnt exist
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

        //If an element is already a target or a source of another element in the list, dont add it to the targets and print an error message.
        if(find_element_by_target_help(&to_add->targets, tok)){
            printf("This target already exists(%s)!\n", tok);
            free(real_tok);
            tok=strtok(NULL, " ");
            continue;
        }
        if (is_target_in_source(to_add->source_friendly, tok))
        {
            printf("Target cannot lie in source!\n");
            free(real_tok);
            tok = strtok(NULL, " ");
            continue;
        }
        if (find_element_by_target(tok))
        {
            printf("Target already used!\n");
            tok = strtok(NULL, " ");
            free(real_tok);
            continue;
        }
       
        //Create new target entry
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

        //Add to the list
        list_target_add(&to_add->targets, new_node);
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
    
    //Setting variables
    int was_source_added = 0;
    char* source_full = realpath(tok, NULL);
    if (source_full == NULL)
    {
        fprintf(stderr, "Source directory doesn't exist '%s': %s\n", tok, strerror(errno));
        return;
    }
    if(find_element_by_target(tok)==1){
        printf("Source is a target of a different backup operation.\n");
        return;
    }

    //Checking whether we have an element with this source already active
    node_sc* source_elem = find_element_by_source(tok);

    //If not create it
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
        fprintf(stderr, "NEW node_sc %p, source='%s'\n", (void*)source_elem, source_elem->source_friendly);
    }

    //Parsing the list of targets
    int cnt = parse_targets(source_elem);

    //If we have added something, and we didn't have the element before add the new node to the list of active whatchers
    if (cnt > 0 && was_source_added)
    {
        list_source_add(source_elem);
    }
    //If the lenght of list of our target is zero delete it
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
    if (&backups == NULL)
        return;

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
void end( char* source_friendly)
{
 
    list_sc* l = &backups;
    char* tok = strtok(NULL, " ");

    //Find the source node
    node_sc* node_found = find_element_by_source(source_friendly);
    if (node_found == NULL)
    {
        puts("No such source");
        return;
    }

    //For every target try to delete it from the list of targets
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

    //If we have deleted all targets from the source node then we can remove it
    if (node_found->targets.size == 0)
    {
        list_source_delete( source_friendly);
    }
    
}

int main()
{
    char* buff = NULL;
    size_t z = 0;
    int n = 0;

    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;

    int k;

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
            end( tok);
        }
        else if (strcmp(tok, "restore") == 0)
        {
        }
        else if (strcmp(tok, "exit") == 0)
        {
            delete_backups_list();
            free(buff);
            return 0;
        }
        else if (strcmp(tok, "list") == 0)
        {
            list_sources_and_targets();
        }
    }
    delete_backups_list();
    free(buff);
    return 0;
}
