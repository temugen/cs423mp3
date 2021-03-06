#ifndef __MP3_INCLUDE__
#define __MP3_INCLUDE__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include "sample.h"
#include "mp3_given.h"

#define DEVICE_NAME "MP3"
#define JIFF_TO_MS(t) (((t)*1000)/ HZ)
#define MS_TO_JIFF(j) (((j) * HZ) / 1000)
#define WORK_PERIOD MS_TO_JIFF(1000 / WORK_HZ)

#define PROC_DIRNAME "mp3"
#define PROC_FILENAME "status"
#define UPDATE_THREAD_NAME "kmp3"
#define WORKQUEUE_NAME "mp3wq"

struct task
{
  unsigned long pid;
  struct list_head task_node;
};

//PROC FILESYSTEM ENTRIES
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *register_task_file;

static struct workqueue_struct *workqueue;
static char *buffer;
static struct sample *current_sample;
static unsigned long last_jiffies;
static int work_done = 0;

static void work_handler(struct work_struct *w);
static DECLARE_DELAYED_WORK(work, work_handler);

static struct cdev vfd;
static dev_t vfd_dev;
static int vfd_open(struct inode *inode, struct file *filp);
static int vfd_release(struct inode *inode, struct file *filp);
static int vfd_mmap(struct file *filp, struct vm_area_struct *vma);
static struct file_operations vfd_ops =
{
    .open = vfd_open,
    .release = vfd_release,
    .mmap = vfd_mmap,
    .owner = THIS_MODULE
};

static LIST_HEAD(task_list);
static DEFINE_MUTEX(list_mutex);
#endif
