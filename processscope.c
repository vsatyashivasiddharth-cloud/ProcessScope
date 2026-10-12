#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_PROCESSES 10000

typedef struct
{
    int pid;
    int ppid;
    char name[256];
} Process;

Process processes[MAX_PROCESSES];
int process_count = 0;


/* Display information about one process */
void inspect_process(int pid)
{
    char path[64];
    char line[1024];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
    {
        printf("Process with PID %d was not found.\n", pid);
        return;
    }

    printf("\n===== Process Information =====\n");

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strncmp(line, "Name:", 5) == 0 ||
            strncmp(line, "State:", 6) == 0 ||
            strncmp(line, "PPid:", 5) == 0 ||
            strncmp(line, "VmRSS:", 6) == 0)
        {
            printf("%s", line);
        }
    }

    printf("===============================\n");

    fclose(file);
}


/* Check whether a directory name contains only digits */
int is_numeric(const char *name)
{
    if (*name == '\0')
        return 0;

    while (*name != '\0')
    {
        if (!isdigit((unsigned char)*name))
            return 0;

        name++;
    }

    return 1;
}


/* Read PID, process name and PPID from /proc/<PID>/stat */
int read_process(int pid)
{
    char path[64];
    char line[1024];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    fclose(file);

    char *open = strchr(line, '(');

    if (open == NULL)
        return 0;

    char *close = strrchr(line, ')');

    if (close == NULL || close <= open)
        return 0;

    int name_length = (int)(close - open - 1);

    if (name_length >= (int)sizeof(processes[0].name))
        name_length = sizeof(processes[0].name) - 1;

    strncpy(processes[process_count].name,
            open + 1,
            name_length);

    processes[process_count].name[name_length] = '\0';

    char state;
    int ppid;

    if (sscanf(close + 1, " %c %d", &state, &ppid) != 2)
        return 0;

    processes[process_count].pid = pid;
    processes[process_count].ppid = ppid;

    process_count++;

    return 1;
}


/* Scan all process directories inside /proc */
void scan_processes(void)
{
    DIR *directory = opendir("/proc");

    if (directory == NULL)
    {
        perror("Error opening /proc");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL)
    {
        if (!is_numeric(entry->d_name))
            continue;

        int pid = atoi(entry->d_name);

        if (pid <= 0)
            continue;

        if (process_count >= MAX_PROCESSES)
            break;

        read_process(pid);
    }

    closedir(directory);
}


/* Count children belonging to a process */
int count_children(int pid)
{
    int count = 0;

    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].ppid == pid)
            count++;
    }

    return count;
}


/* Recursively print child processes */
void print_children(int parent_pid, const char *prefix)
{
    int child_count = count_children(parent_pid);
    int current_child = 0;

    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].ppid != parent_pid)
            continue;

        current_child++;

        int is_last = (current_child == child_count);

        printf("%s", prefix);

        if (is_last)
            printf("└── ");
        else
            printf("├── ");

        printf("%s (%d)\n",
               processes[i].name,
               processes[i].pid);

        char new_prefix[1024];

        if (is_last)
        {
            snprintf(new_prefix,
                     sizeof(new_prefix),
                     "%s    ",
                     prefix);
        }
        else
        {
            snprintf(new_prefix,
                     sizeof(new_prefix),
                     "%s│   ",
                     prefix);
        }

        print_children(processes[i].pid, new_prefix);
    }
}


/* Start the process tree from PID 1 */
void print_tree(void)
{
    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].pid == 1)
        {
            printf("%s (%d)\n",
                   processes[i].name,
                   processes[i].pid);

            print_children(1, "");

            return;
        }
    }

    printf("Process with PID 1 was not found.\n");
}


/* Main function */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n");
        printf("  %s --pid <PID>\n", argv[0]);
        printf("  %s --tree\n", argv[0]);

        return 1;
    }

    /* Process Inspector */
    if (strcmp(argv[1], "--pid") == 0)
    {
        if (argc != 3)
        {
            printf("Usage: %s --pid <PID>\n", argv[0]);
            return 1;
        }

        char *endptr;

        long pid_value = strtol(argv[2], &endptr, 10);

        if (*argv[2] == '\0' || *endptr != '\0')
        {
            printf("Invalid PID.\n");
            return 1;
        }

        if (pid_value <= 0 || pid_value > 2147483647)
        {
            printf("Invalid PID.\n");
            return 1;
        }

        inspect_process((int)pid_value);
    }

    /* Process Tree */
    else if (strcmp(argv[1], "--tree") == 0)
    {
        if (argc != 2)
        {
            printf("Usage: %s --tree\n", argv[0]);
            return 1;
        }

        scan_processes();
        print_tree();
    }

    /* Invalid command */
    else
    {
        printf("Usage:\n");
        printf("  %s --pid <PID>\n", argv[0]);
        printf("  %s --tree\n", argv[0]);

        return 1;
    }

    return 0;
}
