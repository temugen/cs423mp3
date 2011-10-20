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

void _insert_task(struct task* t)
{
    BUG_ON(t == NULL);
    list_add_tail(&t->task_node, &task_list);
}

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

void _destroy_workqueue(void)
{
    cancel_delayed_work(&work);
    flush_workqueue(workqueue);
    destroy_workqueue(workqueue);
    workqueue = NULL;
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
    mutex_unlock(&list_mutex);

    //no more elements in the task list
    if(list_empty(&task_list))
    {
        _destroy_workqueue();
    }
    kfree(t);

    return 0;
}

static void work_handler(struct work_struct *w)
{
    struct list_head *pos;
    struct task *p;
    int i = 0;

    //re-add the work to the queue to ensure 20 Hz
    queue_delayed_work(workqueue, &work, WORK_PERIOD);

    //sum the statistics
    mutex_lock(&list_mutex);
    list_for_each(pos, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        current_sample->timestamp = jiffies;
        get_cpu_use(p->pid, &current_sample->minor_faults, &current_sample->major_faults, &current_sample->utilization);
        i++;
    }
    mutex_unlock(&list_mutex);

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

    //this is the first task in the list
    if(list_empty(&task_list))
    {
        workqueue = create_workqueue(WORKQUEUE_NAME);
        queue_delayed_work(workqueue, &work, WORK_PERIOD);
    }

    mutex_lock(&list_mutex);
    _insert_task(newtask);
    mutex_unlock(&list_mutex);

    return 0;
}

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

static int vfd_mmap(struct file *filp, struct vm_area_struct *vma)
{
    char *addr;
    unsigned long pfn, size = BUFFER_SIZE;
    addr = buffer;

    while(size > 0)
    {
        pfn = vmalloc_to_pfn(addr);
        remap_pfn_range(vma, vma->vm_start + (addr - buffer), pfn, PAGE_SIZE, PAGE_SHARED);
        addr += PAGE_SIZE;

        if(PAGE_SIZE > size)
            size = 0;
        else
            size -= PAGE_SIZE;
    }
    return 0;
}

//ALLOCATE VIRTUALLY CONTIGUOUS MEMORY FOR USER SPACE USE
void *uvmalloc(unsigned long size)
{
    char *mem, *addr;

    size = PAGE_ALIGN(size);
    mem = addr = vmalloc(size);

    if(mem)
    {
        while(size > 0)
        {
            SetPageReserved(vmalloc_to_page(addr));
            addr += PAGE_SIZE;

            //we have an unsigned value, be careful
            if(PAGE_SIZE > size)
                size = 0;
            else
                size -= PAGE_SIZE;
        }
    }
    return mem;
}

void uvfree(void *mem, unsigned long size)
{
    char *addr = mem;

    if(mem)
    {
        while(size > 0)
        {
            ClearPageReserved(vmalloc_to_page(addr));
            addr += PAGE_SIZE;

            if(PAGE_SIZE > size)
                size = 0;
            else
                size -= PAGE_SIZE;
        }
        vfree(mem);
    }
}

//THIS FUNCTION GETS EXECUTED WHEN THE MODULE GETS LOADED
//NOTE THE __INIT ANNOTATION AND THE FUNCTION PROTOTYPE
int __init my_module_init(void)
{
    proc_dir = proc_mkdir(PROC_DIRNAME, NULL);
    register_task_file = create_proc_entry(PROC_FILENAME, 0666, proc_dir);
    register_task_file->read_proc = proc_registration_read;
    register_task_file->write_proc = proc_registration_write;

    buffer = (char *)uvmalloc(BUFFER_SIZE);
    current_sample = (struct sample *)buffer;
    //buffer_pfn = vmalloc_to_pfn(buffer);
    //buffer_page = pfn_to_page(buffer_pfn);

    alloc_chrdev_region(&vfd_dev, 0, 1, DEVICE_NAME);
    cdev_init(&vfd, &vfd_ops);
    vfd.owner = THIS_MODULE;
    vfd.ops = &vfd_ops;
    cdev_add(&vfd, vfd_dev, 1);

    //THE EQUIVALENT TO PRINTF IN KERNEL SPACE
    printk(KERN_ALERT "MODULE LOADED\n");

    return 0;
}

//THIS FUNCTION GETS EXECUTED WHEN THE MODULE GETS UNLOADED
//NOTE THE __EXIT ANNOTATION AND THE FUNCTION PROTOTYPE
void __exit my_module_exit(void)
{
    remove_proc_entry(PROC_FILENAME, proc_dir);
    remove_proc_entry(PROC_DIRNAME, NULL);

    if(workqueue != NULL)
    {
        _destroy_workqueue();
    }

    unregister_chrdev_region(vfd_dev, 1);
    cdev_del(&vfd);

    mutex_lock(&list_mutex);
    _destroy_task_list();
    mutex_unlock(&list_mutex);

    uvfree(buffer, BUFFER_SIZE);

    printk(KERN_ALERT "MODULE UNLOADED\n");
}

//WE REGISTER OUR INIT AND EXIT FUNCTIONS HERE SO INSMOD CAN RUN THEM
//MODULE_INIT AND MODULE_EXIT ARE MACROS DEFINED IN MODULE.H
module_init(my_module_init);
module_exit(my_module_exit);

//THIS IS REQUIRED BY THE KERNEL
MODULE_LICENSE("GPL");
