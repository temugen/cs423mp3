#ifndef __MP3_GIVEN_INCLUDE__
#define __MP3_GIVEN_INCLUDE__

#include <linux/pid.h>

#define find_task_by_pid(nr) pid_task(find_vpid(nr), PIDTYPE_PID)

//THIS FUNCTION RETURNS 0 IF THE PID IS VALID AND THE CPU TIME AND MAJOR AND MINOR PAGE FAULT COUNTS ARE SUCCESFULLY RETURNED BY THE PARAMETER CPU_USE. OTHERWISE IT RETURNS -1
int get_cpu_use(int pid, unsigned long *min, unsigned long *maj, unsigned long *cpu_use)
{
   struct task_struct* task;

   rcu_read_lock();

   task=find_task_by_pid(pid);

   if (task!=NULL) {
     *cpu_use=task->utime + task->stime;
     *maj=task->maj_flt;
     *min=task->min_flt;

     task->utime = task->stime = task->maj_flt = task->min_flt = 0;

     rcu_read_unlock();
     return 0;
   }
   else {
     rcu_read_unlock();
     return -1;
   }
}

#endif
