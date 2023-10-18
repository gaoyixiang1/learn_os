/*
    打印进程的兄弟进程信息以及子进程信息
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>   //task结构体
#include <linux/init_task.h>   
#include <linux/types.h>

static int __init print_pcb_info(void) 
{
        struct task_struct *task,*p,*child,*bro;
        struct list_head *pos,*childpos,*brother;   //双向链表
        //计数器
        int process_count=0;
 
        printk("progress begin...\n");
        task=&init_task;  //指向0号进程pcb
        
        list_for_each(pos,&task->tasks)
        {
                int child_process_count=0,brother_process_count=0;    //计数器

                p=list_entry(pos,struct task_struct,tasks);
                //此时的p指针已经指向task_struct结构体的首部，后面就可以通过p指针进行操作

                process_count++;
                printk("第%d个进程信息如下：\n",process_count);
                printk("name: %s, pid: %d, parent_pid: %d\n",p->comm,p->pid,(p->parent)->pid);
                printk("--------------------------------子进程信息如下-------------------------------------------\n");
                //打印子进程的内容
                 list_for_each(childpos,&p->children){
                     child=list_entry(childpos,struct task_struct,sibling);
                     if(child->pid>0){
                        child_process_count++;
                        printk("进程 %s 的第 %d 个子进程信息：name: %s, pid: %d\n",p->comm,child_process_count,child->comm,child->pid);
                     }
                    
                 }
                 printk("该进程有 %d 个子进程\n",child_process_count);
                 printk("--------------------------------兄弟进程信息如下------------------------------------------\n");
                //打印兄弟进程的内容               
                list_for_each_entry(bro, &(p->parent->children), sibling) {
                    if(bro->pid>0){
                        brother_process_count++;
                        printk("进程 %s 的第 %d 个兄弟进程信息：name: %s, pid: %d\n",p->comm,brother_process_count,bro->comm,bro->pid);
                    }
                
                }
                printk("该进程有 %d 个兄弟进程\n",brother_process_count);
                 printk("-------------------------------- 此进程信息打印完毕------------------------------------------\n");
                 printk("\n");      
        }
        
        printk("进程的个数:%d\n",process_count);
        return 0;
} 
static void __exit exit_pcb_info(void)
{
        printk("goodbye!...\n");
} 
module_init(print_pcb_info);
module_exit(exit_pcb_info);

MODULE_LICENSE("GPL"); 