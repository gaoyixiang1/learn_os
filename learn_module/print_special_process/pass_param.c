/*

传参数打印特定进程的信息

*/


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>   //task结构体
#include <linux/fdtable.h>      //files
#include <linux/fs_struct.h>   //fs
#include <linux/mm_types.h>   //打印内存信息
#include <linux/init_task.h>   
#include <linux/types.h>
#include <linux/atomic.h>
#include<linux/pid.h>
int my_pid = 5;
module_param(my_pid,int,0644);
static int __init my_test_init(void)
{   struct pid* pid = find_get_pid(my_pid);
// if)
   struct task_struct *p;
   p = pid_task(pid,PIDTYPE_PID);
    printk("pid为%d的信息如下：\n",my_pid);
if(p){
     printk("name: %s, pid: %d, state: %d, exit_state: %d, exit_code: %d, exit_signal: %d, parent_pid: %d, parent_name: %s,  utime: %d, stime: %d\n",p->comm,p->pid,p->__state,p->exit_state,p->exit_code,p->exit_signal,(p->parent)->pid,(p->parent)->comm,p->utime,p->stime);
}          
     
    return 0;
}
static void __exit my_test_exit(void)
{
    printk("goodbye\n");
}
module_init(my_test_init);
module_exit(my_test_exit);
MODULE_LICENSE("GPL");