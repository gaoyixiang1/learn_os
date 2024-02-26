#include <linux/sched/signal.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/init_task.h>
#include <linux/types.h>
#include <linux/dcache.h>
#include <linux/pid.h>
#include <linux/fs_struct.h>

int my_pid = 5;
module_param(my_pid, int, 0644);

static int __init print_process_fs_info_init(void)
{
    struct pid *pid = find_get_pid(my_pid);
    struct task_struct *p;

    p = pid_task(pid, PIDTYPE_PID);//获得当前pid的tsk

    // 保存当前进程的fs,files
    struct fs_struct *fs;
    struct files_struct *files;

    // fs
    struct path file_path, pwd;
    // files
    struct fdtable *fdtab;
    // ->file
    struct file __rcu **fd;
    struct file *file;
    // inode
    struct inode *inode;
    fmode_t f_mode; // 文件打开模式
    // path
    struct dentry *dentry;

    printk("pid为%d的信息如下：\n", my_pid);
    if (p)
    {
        printk("name: %s, pid: %d\n", p->comm, p->pid);

printk("fs info***********************************************************************************************\n");
        fs = p->fs, files = p->files;
        fdtab =  files_fdtable(p->files);

        printk(KERN_INFO"当前工作目录路径为 %s \n",fs->pwd.dentry->d_name.name);
        printk(KERN_INFO "用户数目为: %d\n", fs->users);
 printk("files info---------------------------------------------\n");
         char tmp[4096];
        int i = 0;
        for (i = 0; i < fdtab->max_fds; i++)
        {
            file = fdtab->fd[i]; 
            if (file)
            {
                //获取文件名及其相应的路径
                file_path = file->f_path;
                char *tmp;
                path_get(&file_path); // Increment reference count of the path,增加路径引用次数
                tmp = (char *)__get_free_page(GFP_KERNEL);
                if (!tmp) {
                    printk(KERN_INFO "Memory allocation failed\n");
                    return -ENOMEM;
                }
                char *filepath;
                filepath = d_path(&file_path, tmp, PAGE_SIZE);
                if (IS_ERR(filepath)) 
                { // Make sure d_path() didn't fail
                    free_page((unsigned long)tmp);
                    continue;
                }

                dentry = file_path.dentry;//dentry；
                inode = file_inode(file);

                printk(KERN_INFO "文件名为: %s\n", filepath);
                printk("文件描述符为：%d ,引用计数: %d, 文件标志: %x, 文件打开模式: %o  \n",i,file->f_count.counter, file->f_flags, file->f_mode);
                printk("dentry info----------------------------------------------------\n");
                printk(KERN_INFO " 父级目录项名: %s\n", dentry->d_parent->d_name.name);
                printk(KERN_INFO " 当前目录项名: %s\n", dentry->d_name.name);
                printk(KERN_INFO " 目录项关联的inode的id: %lu\n", dentry->d_inode->i_ino);
                printk(KERN_INFO "inode info:---------------------------------------------\n");
                printk(KERN_INFO " 节点号: %lu\n", inode->i_ino);
                printk(KERN_INFO " 硬链接数: %u\n", inode->i_nlink);
                printk(KERN_INFO " 实际设备标识符: %lu\n", inode->i_rdev);
                printk(KERN_INFO " 块大小: %u\n", inode->i_blkbits);

                printk(KERN_INFO " 块个数: %llu\n", inode->i_blocks);
                printk(KERN_INFO " 引用计数: %d\n", atomic_read(&inode->i_count));

                free_page((unsigned long)tmp);
                path_put(&file_path);
            }
        }

        
    }

    return 0;
}

static void __exit print_process_fs_info_exit(void)
{
    printk("goodbye\n");
}

module_init(print_process_fs_info_init);
module_exit(print_process_fs_info_exit);
MODULE_LICENSE("GPL");
