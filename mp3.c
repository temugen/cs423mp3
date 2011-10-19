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

    if(list_empty(&task_list))
    {
        destroy_workqueue(workqueue);
    }

    kfree(t);

    return 0;
}

int proc_registration_read(char *page, char **start, off_t off, int count, int* eof, void* data)
{
    off_t i;
    struct list_head *pos;
    struct task *p;

    i = 0;
    mutex_lock(&list_mutex);
    list_for_each(pos, &task_list)
    {
        p = list_entry(pos, struct task, task_node);
        i += sprintf(page+off+i, "%lu\n", p->pid);
    }
    mutex_unlock(&list_mutex);
    *eof = 1;

    return i;
}

//ALLOCATE AND POPULATE TASK, ADD TO LIST
int register_task(unsigned long pid)
{
    struct task* newtask;

    if (_lookup_task(pid) != NULL) return -1;

    newtask = kmalloc(sizeof(struct task), GFP_KERNEL);
    newtask->pid = pid;
    newtask->linux_task = find_task_by_pid(pid);

    if(list_empty(&task_list))
    {
        workqueue = create_workqueue(WORKQUEUE_NAME);
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

//THIS FUNCTION GETS EXECUTED WHEN THE MODULE GETS LOADED
//NOTE THE __INIT ANNOTATION AND THE FUNCTION PROTOTYPE
int __init my_module_init(void)
{
    proc_dir = proc_mkdir(PROC_DIRNAME, NULL);
    register_task_file = create_proc_entry(PROC_FILENAME, 0666, proc_dir);
    register_task_file->read_proc = proc_registration_read;
    register_task_file->write_proc = proc_registration_write;

    buffer = (char *)vmalloc(BUFFER_SIZE);

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

    _destroy_task_list();

    vfree(buffer);

    printk(KERN_ALERT "MODULE UNLOADED\n");
}

//WE REGISTER OUR INIT AND EXIT FUNCTIONS HERE SO INSMOD CAN RUN THEM
//MODULE_INIT AND MODULE_EXIT ARE MACROS DEFINED IN MODULE.H
module_init(my_module_init);
module_exit(my_module_exit);

//THIS IS REQUIRED BY THE KERNEL
MODULE_LICENSE("GPL");
