#ifndef __MP3_INCLUDE__
#define __MP3_INCLUDE__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <asm/uaccess.h>

#include "mp3_given.h"

#define JIFF_TO_MS(t) ((t*1000)/ HZ)
#define MS_TO_JIFF(j) ((j * HZ) / 1000)

#define PROC_DIRNAME "mp3"
#define PROC_FILENAME "status"
#define UPDATE_THREAD_NAME "kmp3"
#define WORKQUEUE_NAME "mp3wq"

//4KB buffer
#define BUFFER_SIZE (128 * 4 * 1024)

struct task
{
  unsigned long pid, utilization, major_faults, minor_faults;
  struct list_head task_node;
  struct task_struct *linux_task;
};

//PROC FILESYSTEM ENTRIES
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *register_task_file;

struct workqueue_struct *workqueue;
char *buffer;

LIST_HEAD(task_list);
static DEFINE_MUTEX(list_mutex);
#endif
