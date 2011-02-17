/*
 *  iowaittop/iowaittop.c
 *
 *  Copyright (c) 2011 Mozilla Foundation
 *  Patrick Walton <pcwalton@mozilla.com>
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bstrlib.h"

#define DISPLAYED_TASK_COUNT 5

struct task {
    pid_t pid;
    unsigned iowait_count;
};

struct delta {
    pid_t pid;
    unsigned iowait_count_delta;
};

int compare_pid_to_task(const void *key, const void *member)
{
    const pid_t *pid = key;
    const struct task *task = member;
    return *pid - task->pid;
}

int compare_deltas(const void *a, const void *b)
{
    const struct delta *delta_a = a, *delta_b = b;
    return delta_b->iowait_count_delta - delta_a->iowait_count_delta;
}

int compare_tasks(const void *a, const void *b)
{
    const struct task *task_a = a, *task_b = b;
    return task_a->pid - task_b->pid;
}

bool get_iowait_count(const char *path, unsigned *count)
{
    FILE *f;
    if (!(f = fopen(path, "r")))
        return false;

    bstring line;
    bool ok = false;
    while ((line = bgets((bNgetc)fgetc, f, '\n'))) {
        if (sscanf(bdata(line), "se.iowait_count : %u", count)) {
            ok = true;
            break;
        }
    }

    fclose(f);
    return ok;
}

bstring name_of(pid_t pid)
{
    bstring path = bformat("/proc/%u/cmdline", (unsigned)pid);
    if (!path)
        return NULL;
    FILE *f = fopen(bdata(path), "r");
    bdestroy(path);
    if (f) {
        bstring result = bgets((bNgetc)fgetc, f, '\n');
        fclose(f);
        if (result && blength(result))
            return result;
    }

    path = bformat("/proc/%u/status", (unsigned)pid);
    if (!path)
        return NULL;
    f = fopen(bdata(path), "r");
    bdestroy(path);
    if (!f)
        return NULL;

    struct tagbstring name_str = bsStatic("Name:\t");
    bstring line;
    bstring result = NULL;
    while ((line = bgets((bNgetc)fgetc, f, '\n'))) {
        if (binstr(line, 0, &name_str) != 0) {
            bdestroy(line);
            continue;
        }

        result = bmidstr(line, blength(&name_str), blength(line) -
            blength(&name_str) - 1);
        break;
    }

    fclose(f);
    return result;
}

void display(const_bstring deltas)
{
    printf("---\n");
    printf("IOWAIT-COUNT  PID     NAME\n");

    int i;
    for (i = 0; i < DISPLAYED_TASK_COUNT; i++) {
        if (i >= blength(deltas) / sizeof(struct delta))
            break;
        struct delta *delta = ((struct delta *)bdata(deltas)) + i;

        bstring name = name_of(delta->pid);
        printf("%12u  %6u  %s\n", delta->iowait_count_delta,
            (unsigned)delta->pid, name ? bdata(name) : "???");
        if (name)
            bdestroy(name);
    }
}

int main()
{
    bstring tasks = NULL;
    while (1) {
        bstring new_tasks = bfromcstr("");
        bstring deltas = bfromcstr("");

        DIR *dir = opendir("/proc");
        if (!dir) {
            perror("couldn't open /proc");
            return 1;
        }

        int task_count = blength(tasks) / sizeof(struct task);
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            struct task task;
            if (!(task.pid = strtol(ent->d_name, NULL, 0)))
                continue;

            bstring path = bformat("/proc/%s/sched", ent->d_name);
            bool iowait_present = get_iowait_count(bdata(path),
                &task.iowait_count);
            bdestroy(path);
            if (!iowait_present)
                continue;

            if (tasks) {
                struct task *old_task = bsearch(&task.pid, bdata(tasks),
                    task_count, sizeof(struct task), compare_pid_to_task);
                if (old_task) {
                    struct delta delta;
                    delta.pid = task.pid;
                    delta.iowait_count_delta = task.iowait_count -
                        old_task->iowait_count;
                    if (bcatblk(deltas, &delta, sizeof(delta)) != BSTR_OK)
                        return 1;
                }
            }

            if (bcatblk(new_tasks, &task, sizeof(task)) != BSTR_OK)
                return 1;
        }

        closedir(dir);

        qsort(bdata(deltas), blength(deltas) / sizeof(struct delta),
            sizeof(struct delta), compare_deltas);
        display(deltas);

        qsort(bdata(tasks), blength(tasks) / sizeof(struct task),
            sizeof(struct task), compare_tasks);

        if (tasks)
            bdestroy(tasks);
        tasks = new_tasks;

        sleep(1);
    }

    return 0;   // not reached
}

