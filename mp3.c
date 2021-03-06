#include "mp3.h"

//REMOVE ALL TASKS FROM THE LIST, RESET THEIR SCHEDULING, AND FREE THEM
void _destroy_task_list(void)
{
    struct list_head *pos, *tmp;
    struct task *p;

    list_for_each_safe(pos, tmp, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        list_del(pos);
        kfree(p);
    }
}

//INSERTS A TASK INTO OUR LINKED LIST
void _insert_task(struct task* t)
{
    BUG_ON(t == NULL);
    list_add_tail(&t->task_node, &task_list);
}

//FINDS A TASK BY PID
struct task* _lookup_task(unsigned long pid)
{
    struct list_head *pos;
    struct task *p;

    list_for_each(pos, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        if(p->pid == pid)
            return p;
    }

    return NULL;
}

//REMOVE TASK FROM LIST, MARK FOR DEREGISTRATION, CONTEXT SWITCH
int deregister_task(unsigned long pid)
{
    struct task *t;

    mutex_lock(&list_mutex);
    if((t = _lookup_task(pid)) == NULL)
    {
        mutex_unlock(&list_mutex);
        return -1;
    }

    list_del(&t->task_node);

    //no more elements in the task list
    if(list_empty(&task_list))
    {
        //remove all work
        work_done = 1;
        cancel_delayed_work(&work);
        //mark the end of our statistics
        current_sample->timestamp = -1;
        current_sample = (struct sample *)buffer;
    }
    mutex_unlock(&list_mutex);
    kfree(t);

    return 0;
}

//HANDLES SAMPLING EVERY WORK_PERIOD
static void work_handler(struct work_struct *w)
{
    struct list_head *pos;
    struct task *p;
    unsigned long minor_faults, major_faults, utilization;
    int i = 0;

    //re-add the work to the queue to ensure 20 Hz
    queue_delayed_work(workqueue, &work, WORK_PERIOD);

    if(work_done) {
        cancel_delayed_work(&work);
    }

    current_sample->timestamp = jiffies;
    current_sample->major_faults = current_sample->minor_faults = current_sample->utilization = 0;
    //sum the statistics
    mutex_lock(&list_mutex);
    list_for_each(pos, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        get_cpu_use(p->pid, &minor_faults, &major_faults, &utilization);
        current_sample->minor_faults += minor_faults;
        current_sample->major_faults += major_faults;
        current_sample->utilization += utilization;
        i++;
    }
    mutex_unlock(&list_mutex);
    current_sample->utilization *= 100;
    current_sample->utilization /= (current_sample->timestamp - last_jiffies);
    last_jiffies = jiffies;

    if(i > 0)
    {
        //move the current sample down the ring buffer
        current_sample++;
        if(current_sample >= (struct sample *)(buffer + BUFFER_SIZE))
            current_sample = (struct sample *)buffer;
    }
    else
    {
        printk("WARNING: Workqueue found no tasks.\n");
    }
}

//PRINT INFO WHEN PROC FILE IS READ
int proc_registration_read(char *page, char **start, off_t off, int count, int* eof, void* data)
{
    off_t i;
    struct list_head *pos;
    struct task *p;

    //print the PIDs of the registered tasks.
    i = 0;
    mutex_lock(&list_mutex);
    list_for_each(pos, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        i += sprintf(page + off + i, "%lu\n", p->pid);
    }
    mutex_unlock(&list_mutex);
    *eof = 1;

    return i;
}

//ALLOCATE AND POPULATE TASK, ADD TO LIST
int register_task(unsigned long pid)
{
    struct task* newtask;
    unsigned long temp;

    mutex_lock(&list_mutex);
    //we already have this task registered
    if (_lookup_task(pid) != NULL)
    {
        mutex_unlock(&list_mutex);
        return -1;
    }
    mutex_unlock(&list_mutex);

    newtask = kmalloc(sizeof(struct task), GFP_KERNEL);
    newtask->pid = pid;
    //reset the statistics
    get_cpu_use(pid, &temp, &temp, &temp);

    mutex_lock(&list_mutex);
    //this is the first task in the list
    if(list_empty(&task_list))
    {
        work_done = 0;
        last_jiffies = jiffies;
        queue_delayed_work(workqueue, &work, WORK_PERIOD);
    }

    _insert_task(newtask);
    mutex_unlock(&list_mutex);

    return 0;
}

//HANDLES REGISTRATION AND DEREGISTRATION
int proc_registration_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char *proc_buffer;
    char reg_type;
    unsigned long pid;

    proc_buffer = kmalloc(count, GFP_KERNEL);
    if(copy_from_user(proc_buffer, buffer, count) != 0)
        goto copy_fail;

    reg_type = proc_buffer[0];
    switch(reg_type)
    {
        case 'R':
            sscanf(proc_buffer, "%c %lu", &reg_type, &pid);
            register_task(pid);
            printk(KERN_ALERT "Register Task:%lu\n", pid);
            break;
        case 'U':
            sscanf(proc_buffer, "%c %lu", &reg_type, &pid);
            deregister_task(pid);
            printk(KERN_ALERT "Deregister Task:%lu\n", pid);
            break;
        default:
            printk(KERN_ALERT "Malformed registration string\n");
            break;
    }

copy_fail:
    kfree(proc_buffer);
    return count;
}

static int vfd_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vfd_release(struct inode *inode, struct file *filp)
{
    return 0;
}

//MAPS OUR BUFFER TO USER SPACE ADDRESSES
static int vfd_mmap(struct file *filp, struct vm_area_struct *vma)
{
    return remap_vmalloc_range(vma, buffer, 0);
}

//THIS FUNCTION GETS EXECUTED WHEN THE MODULE GETS LOADED
//NOTE THE __INIT ANNOTATION AND THE FUNCTION PROTOTYPE
int __init my_module_init(void)
{
    //proc entry
    proc_dir = proc_mkdir(PROC_DIRNAME, NULL);
    register_task_file = create_proc_entry(PROC_FILENAME, 0666, proc_dir);
    register_task_file->read_proc = proc_registration_read;
    register_task_file->write_proc = proc_registration_write;

    //buffer for userspace
    buffer = (char *)vmalloc_user(BUFFER_SIZE);
    current_sample = (struct sample *)buffer;

    //chrdev
    alloc_chrdev_region(&vfd_dev, 0, 1, DEVICE_NAME);
    cdev_init(&vfd, &vfd_ops);
    vfd.owner = THIS_MODULE;
    vfd.ops = &vfd_ops;
    cdev_add(&vfd, vfd_dev, 1);

    //workqueue
    workqueue = create_workqueue(WORKQUEUE_NAME);

    //THE EQUIVALENT TO PRINTF IN KERNEL SPACE
    printk(KERN_ALERT "MODULE LOADED with BUFFER_SIZE=%lu\n", BUFFER_SIZE);

    return 0;
}

//THIS FUNCTION GETS EXECUTED WHEN THE MODULE GETS UNLOADED
//NOTE THE __EXIT ANNOTATION AND THE FUNCTION PROTOTYPE
void __exit my_module_exit(void)
{
    //proc entry
    remove_proc_entry(PROC_FILENAME, proc_dir);
    remove_proc_entry(PROC_DIRNAME, NULL);

    //work queue
    work_done = 1;
    cancel_delayed_work(&work);
    flush_workqueue(workqueue);
    destroy_workqueue(workqueue);

    //chrdev
    unregister_chrdev_region(vfd_dev, 1);
    cdev_del(&vfd);

    //task list
    mutex_lock(&list_mutex);
    _destroy_task_list();
    mutex_unlock(&list_mutex);

    //buffer
    vfree(buffer);

    printk(KERN_ALERT "MODULE UNLOADED\n");
}

//WE REGISTER OUR INIT AND EXIT FUNCTIONS HERE SO INSMOD CAN RUN THEM
//MODULE_INIT AND MODULE_EXIT ARE MACROS DEFINED IN MODULE.H
module_init(my_module_init);
module_exit(my_module_exit);

//THIS IS REQUIRED BY THE KERNEL
MODULE_LICENSE("GPL");
