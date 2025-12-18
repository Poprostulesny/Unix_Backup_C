#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "generic_file_functions.h"
#include "inotify_functions.h"
#include "restore.h"
#include "utils.h"

#include "list_move_events.h"
#include "list_sources.h"
#include "list_targets.h"
#include "lists_common.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define DEBUG

volatile sig_atomic_t finish_work_flag = 0;
volatile sig_atomic_t restore_expected = 0;
volatile sig_atomic_t restore_received = 0;
volatile sig_atomic_t child_count = 0;

/* GLOBAL VARIABLES */
char* _source;
char* _source_friendly;
char* _target;

/*-------------------*/

void stop_all_backups(void);
void block_all_signals(void);
void unblock_sigint_sigterm(void);
void spawn_child_for_target(node_sc* source_node, node_tr* target_node);
void child_loop(node_sc* source_node, node_tr* target_node);
void handle_end_for_target(node_sc* source_node, node_tr* target_node);
void inotify_jobs(node_sc* source_node, node_tr* target_node);

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    act.sa_flags = 0;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}
void sig_handler(int sig) { finish_work_flag = 1; }

void sigusr1_parent(int sig)
{
    (void)sig;
    restore_received++;
}

volatile sig_atomic_t child_pause_requested = 0;
volatile sig_atomic_t child_ack_sent = 0;
volatile sig_atomic_t child_should_exit = 0;
const char* child_target_name = NULL;

void sigusr1_child(int sig)
{
    (void)sig;
    child_pause_requested ^= 1;
}

void sigusr2_child(int sig)
{
    (void)sig;
    child_should_exit = 1;
}

void block_all_signals()
{
    sigset_t set;
    if (sigfillset(&set) == -1)
    {
        ERR("sigfillset");
    }
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
    {
        ERR("sigprocmask block all");
    }
}

void unblock_sigint_sigterm()
{
    sigset_t set;
    if (sigemptyset(&set) == -1)
    {
        ERR("sigemptyset");
    }
    if (sigaddset(&set, SIGINT) == -1 || sigaddset(&set, SIGTERM) == -1 || sigaddset(&set, SIGUSR1) == -1)
    {
        ERR("sigaddset");
    }
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
    {
        ERR("sigprocmask unblock");
    }
}

void child_loop(node_sc* source_node, node_tr* target_node)
{
    block_all_signals();
    sethandler(sigusr1_child, SIGUSR1);
    sethandler(sigusr2_child, SIGUSR2);
    sigset_t child_unblock;
    if (sigemptyset(&child_unblock) == -1)
    {
        ERR("sigemptyset child");
    }
    if (sigaddset(&child_unblock, SIGUSR1) == -1 || sigaddset(&child_unblock, SIGUSR2) == -1)
    {
        ERR("sigaddset child");
    }
    if (sigprocmask(SIG_UNBLOCK, &child_unblock, NULL) == -1)
    {
        ERR("sigprocmask child unblock");
    }

    child_target_name = target_node->target_friendly;
    initial_backup(source_node, target_node);

    while (!child_should_exit)
    {
        if (child_pause_requested && !child_ack_sent)
        {
            if (child_target_name != NULL)
            {
                fprintf(stderr, "[DEBUG] child '%s' paused for restore\n", child_target_name);
            }
            kill(getppid(), SIGUSR1);
            child_ack_sent = 1;
        }
        if (child_pause_requested)
        {
            sigset_t pause_mask;
            sigfillset(&pause_mask);
            sigdelset(&pause_mask, SIGUSR1);
            sigdelset(&pause_mask, SIGUSR2);
            sigsuspend(&pause_mask);
            continue;
        }
        child_ack_sent = 0;
        if (child_target_name != NULL)
        {
            fprintf(stderr, "[DEBUG] child '%s' resumed after restore\n", child_target_name);
        }

        inotify_jobs(source_node, target_node);

        if (child_should_exit)
        {
            break;
        }
        sleep(1);
    }
    delete_target_node(target_node);
    _exit(EXIT_SUCCESS);
}

void spawn_child_for_target(node_sc* source_node, node_tr* target_node)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        ERR("fork");
    }
    if (pid == 0)
    {
        child_loop(source_node, target_node);
    }
    if (setpgid(pid, getpid()) == -1)
    {
        ERR("setpgid");
    }
    target_node->child_pid = pid;
    child_count++;
}

void handle_end_for_target(node_sc* source_node, node_tr* target_node)
{
    if (target_node == NULL || source_node == NULL)
    {
        return;
    }
    if (target_node->child_pid > 0)
    {
        kill(target_node->child_pid, SIGUSR2);
        waitpid(target_node->child_pid, NULL, 0);
        child_count--;
    }
    list_target_delete(&source_node->targets, target_node->target_friendly);
}

/*


    ADD


*/
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
    char* tok = tokenizer(NULL);
    int cnt = 0;
    while (tok != NULL)
    {
        // If the directory is not empty, write and continue parsing, is_empty_dir ensures that it is empty or creates
        // it if it doesnt exist
        if (is_empty_dir(tok) < 0)
        {
            printf("Target has to be an empty directory!\n");
            tok = tokenizer(NULL);
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
            tok = tokenizer(NULL);
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
        spawn_child_for_target(to_add, new_node);
#ifdef DEBUG
        printf("Full source: '%s', friendly: '%s'\n", to_add->source_full, to_add->source_friendly);

#endif
        cnt++;
        tok = tokenizer(NULL);
        free(real_tok);
    }
#ifdef DEBUG
    puts("Finished parsing targets");
#endif

    return cnt;
}

int take_input()
{
    char* tok = tokenizer(NULL);
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
        free(source_full);
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
#ifdef DEBUG
        fprintf(stderr, "NEW node_sc %p, source='%s'\n", (void*)source_elem, source_elem->source_full);
#endif
    }

    // Parsing the list of targets
    int cnt = parse_targets(source_elem);

    // If we have added something, and we didn't have the element before add the new node to the list of active
    // whatchers
    if (cnt > 0 && was_source_added)
    {
        list_source_add(source_elem);
    }
    // If the length of list of our target is zero delete it
    else if (source_elem->targets.size == 0)
    {
        delete_source_node(source_elem);
    }

    free(source_full);
#ifdef DEBUG
    puts("Finished taking input");
#endif

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

void stop_all_backups(void)
{
    finish_work_flag = 1;
    kill(-getpid(), SIGUSR2);

    while (wait(NULL) > 0)
    {
        ;
    }
    child_count = 0;
    delete_backups_list();
}

/*


END



*/
void end(char* source_friendly)
{
    char* tok = tokenizer(NULL);

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
        node_tr* cur = node_found->targets.head;
        while (cur != NULL && strcmp(cur->target_friendly, tok) != 0)
        {
            cur = cur->next;
        }
        if (cur != NULL)
        {
            handle_end_for_target(node_found, cur);
        }
        else
        {
            printf("Target not found: %s\n", tok);
        }
        cnt++;
        tok = tokenizer(NULL);
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

// Function to handle inotify events per source node
void inotify_jobs(node_sc* source_node, node_tr* target_node)
{
    if (source_node == NULL || target_node == NULL)
    {
        return;
    }
    if (target_node->inotify_fd < 0)
    {
        return;
    }
    inotify_reader(target_node->inotify_fd, &target_node->watchers, &target_node->events);
    event_handler(source_node, target_node);
    check_move_events_list(&target_node->mov_dict, source_node, target_node);
}

// Function handling the restore request
void restore(char* tok)
{
    (void)tok;

    char* source_tok = tokenizer(NULL);
    char* target_tok = tokenizer(NULL);

    // Checking whether given sources exist
    char* real_source = realpath(source_tok, NULL);
    if (real_source == NULL)
    {
        if (errno == ENOENT)
        {
            make_path(source_tok);
            checked_mkdir(source_tok);
            real_source = realpath(source_tok, NULL);
            if (real_source == NULL)
            {
                return;
            }
        }
        else if (errno == ENOTDIR)
        {
            printf("Source is not a directory!\n");
        }
        else
        {
            printf("Invalid input\n");
            return;
        }
    }
    char* real_target = realpath(target_tok, NULL);
    if (real_target == NULL)
    {
        if (errno == ENOENT)
        {
            make_path(target_tok);
            checked_mkdir(target_tok);
            real_target = realpath(target_tok, NULL);
            if (real_target == NULL)
            {
                return;
            }
        }
        else if (errno == ENOTDIR)
        {
            printf("Target is not a directory!\n");
        }
        else
        {
            printf("Invalid input\n");
            return;
        }
    }

    int expected_waiting = child_count;
    restore_expected = expected_waiting;
    restore_received = 0;
    if (expected_waiting > 0)
    {
        sigset_t set, old;
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        sigaddset(&set, SIGCHLD);
        if (sigprocmask(SIG_BLOCK, &set, &old) == -1)
        {
            ERR("sigprocmask block usr1");
        }
        // broadcast pause to all in our process group
        kill(-getpid(), SIGUSR1);
        while (restore_received < restore_expected)
        {
            siginfo_t info;
            int ret = sigwaitinfo(&set, &info);
            if (ret == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                ERR("sigwaitinfo");
            }
            if (info.si_pid != getpid())
            {
                restore_received++;
            }
            if (info.si_signo == SIGCHLD)
            {
                waitpid(-1, NULL, WNOHANG);
            }
            sleep(1);
        }
        if (sigprocmask(SIG_SETMASK, &old, NULL) == -1)
        {
            ERR("sigprocmask restore");
        }
    }
    // Wrapper to restore the checkpoint
    restore_checkpoint(real_source, real_target);

    if (expected_waiting > 0)
    {
        kill(-getpid(), SIGUSR1);
    }
    free(real_source);
    free(real_target);
}

// Function to handle input from stdin
void input_handler()
{
    size_t z = 0;
    char* buff = NULL;
    int n = 0;
    int k;
    while ((n = getline(&buff, &z, stdin)) != -1)
    {
        if (finish_work_flag)
        {
            break;
        }
        if (buff[n - 1] == '\n')
        {
            buff[n - 1] = '\0';
        }
        if (buff[0] == '\0')
        {
            continue;
        }

        char* tok = tokenizer(buff);
        if (tok == NULL)
        {
            printf("Invalid syntax\n");
            continue;
        }
        // input in the form add <source path> <target path> with multiple target paths
        if (strcmp(tok, "add") == 0)
        {
            if ((k = take_input()) <= 0)
            {
                // do smth
                puts("Invalid syntax\n");
                continue;
            }
        }
        // input in the form end <source path> <target path> with multiple target paths
        else if (strcmp(tok, "end") == 0)
        {
            tok = tokenizer(NULL);
            if (tok == NULL)
            {
                puts("Invalid syntax\n");
                continue;
            }

            end(tok);
        }
        // input in the form restore <source path> <target path> with singular target path
        else if (strcmp(tok, "restore") == 0)
        {
            restore(tok);
        }
        else if (strcmp(tok, "exit") == 0)
        {
            break;
        }
        else if (strcmp(tok, "list") == 0)
        {
            list_sources_and_targets();
        }
    }

    stop_all_backups();
    free(buff);
}

int main()
{
    block_all_signals();
    init_lists();
    sethandler(sig_handler, SIGTERM);
    sethandler(sig_handler, SIGINT);
    sethandler(sigusr1_parent, SIGUSR1);
    unblock_sigint_sigterm();

    input_handler();
    while (wait(NULL) > 0)
    {
        ;
    }
    return 0;
}
