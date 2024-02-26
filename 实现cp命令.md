# 一、实现cp命令

```
请编写一个实现cp命令的简单程序，其中涉及open(),read(),write()和close()等系统调用，并通过命令行的形式进行传参，源文件和目标文件位于不同的文件系统，运行该代码，截图，并根据所学原理深入到内核进行分析。

1. 运行结果正确 3分，并有GDB调试2分
2. 在上次分析open的基础上，基于文件系统缓冲区的认识，对read()和write()进行分析，因为没有学块设备，不要分析的太深入，画流程图（8分）。并给出心得体会（3分）。
```



## 什么是cp命令

在虚拟机中输入`man cp`，其中对cp命令的描述将源文件复制到目标文件，或者将多个源文件复制到目录。在这里简单实现一下将源文件复制到目标文件。

<img src=".\img\cp_1.png" alt="image-20231112192947303" style="zoom:67%;" />

## 从源文件到目标文件需要哪几步？

1.**打开源文件和目标文件：** 在这块使用`open()`来实现打开文件

- 如果打开源文件失败，程序输出错误信息并退出。
- 如果打开目标文件失败，程序输出错误信息，关闭源文件描述符，然后退出。

2.**读取源文件并写入目标文件：**

- 使用`read()`读取源文件内容到缓冲区`buffer`中
- 使用`write()`来把`buffer`中的内容写到目标文件中
- 如果写入的字节数与读取的字节数不一致，说明写入时出现错误，程序输出错误信息，关闭文件描述符，然后退出

3.**关闭文件描述符**

- 使用`close()`函数关闭源文件和目标文件的文件描述符。
- 如果关闭文件描述符失败，程序输出错误信息并退出。

## 代码实现

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    int fd1, fd2;//fd1是打开源文件的文件描述符，fd2是打开目的文件的文件描述符
    ssize_t rd, wt;//rd是读入的字节数，wt是写入的字节数
    char buffer[BUFFER_SIZE];//缓冲区大小

    // 打开源文件
    fd1 = open(argv[1], O_RDONLY);
    if (fd1 == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // 创建目标文件，若已存在则覆盖
    fd2 = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd2 == -1) {
        perror("open");
        close(fd1);
        exit(EXIT_FAILURE);
    }

    // 读取源文件并写入目标文件
    while ((rd = read(fd1, buffer, BUFFER_SIZE)) > 0) {
        wt = write(fd2, buffer, rd);
        if (wt != rd) {
            perror("write");
            close(fd1);
            close(fd2);
            exit(EXIT_FAILURE);
        }
    }

    if (rd == -1) {
        perror("read");
        close(fd1);
        close(fd2);
        exit(EXIT_FAILURE);
    }

    // 关闭文件描述符
    if (close(fd1) == -1 || close(fd2) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    printf("finish copy!!!\n");

    return 0;
}
```

## 编译&调试

<img src=".\img\cp_2.png" alt="image-20231112201926930" style="zoom: 80%;" />



**注意** gdb中并通过命令行的形式进行传参，在这里使用`set args arg1 arg2...`，`break`用来打断点，`print var_name`可以查看对应变量名的内容

<img src=".\img\cp_3.png" style="zoom:67%;" />


<img src=".\img\cp_4.png" alt="image-20231112202257455" style="zoom: 80%;" />

<img src=".\img\cp_5.png" style="zoom:67%;" />

## 运行结果

<img src=".\img\cp_6.png" style="zoom:67%;" />



## 挂载到不同文件系统

copy文件夹在根目录下，其文件系统类型:/dev/sda3。我们本次选择把copy/test.txt放入到/run/lock里面，此/run/lock类型为tmpfs

<img src=".\img\cp_7.png" style="zoom:67%;" />

#### gdb调试

此次将copy目录下的test.txt内容复制到/run/lock/tt.txt中

<img src=".\img\cp_8.png" style="zoom:67%;" />

#### copy结果

<img src=".\img\cp_9.png" alt="image-20231113093714641" style="zoom: 80%;" />

<img src=".\img\cp_10.png" style="zoom:67%;" />



# 二、read && write

根据系统调用的命名规则，read和write的系统调用函数应该是sys_read()及sys_write()，均位于`/fs/read_write.c`中，下面就着重分析一下这两个函数。

## sys_read()

此函数最终是通过ksys_read函数来实现读取文件，ksys_read的参数分别是`unsigned int  fd, char __user *  buf, size_t  count`

```c
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
	return ksys_read(fd, buf, count);
}
```

### ksys_read()

ksys_read()的参数分别为指向文件的指针file,缓冲区buf指针，读入的数据长度count

```c
ssize_t ksys_read(unsigned int fd, char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);//通过fdget_pos来获取相应的文件描述符对应的结构
	ssize_t ret = -EBADF;
//相应的文件描述符对应的文件对象存在
	if (f.file) { 
		loff_t pos, *ppos = file_ppos(f.file);//ppos指向当前文件的文件指针
		if (ppos) {
			pos = *ppos;//pos保存了文件的位移量
			ppos = &pos;
		}
		ret = vfs_read(f.file, buf, count, ppos);//这里主要通过vfs_read来读取文件
        // 如果读取成功且文件的偏移量有效，则更新文件的偏移量
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
         // 释放文件描述符
		fdput_pos(f);
	}
	return ret;
}
```

其中fd结构的定义如下：

```c
struct fd {
	struct file *file;//指向文件结构的指针
	unsigned int flags;
};
```

file_ppos()定义如下：其功能是返回file指针对应的f_pos，即文件当前的位移量

```
/* file_ppos returns &file->f_pos or NULL if file is stream */
static inline loff_t *file_ppos(struct file *file)
{
	return file->f_mode & FMODE_STREAM ? NULL : &file->f_pos;
}
```

### vfs_read()

vfs_read()函数的参数分别为指向文件的指针file,缓冲区buf指针，读入的数据长度count以及文件偏移量指针pos

```c
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;
//权限检查
	if (!(file->f_mode & FMODE_READ))//文件不允许读
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_READ))//文件是否可读
		return -EINVAL;
	if (unlikely(!access_ok(buf, count)))//是否可以访问buf
		return -EFAULT;
//调用 rw_verify_area 函数验证读取区域的有效性
	ret = rw_verify_area(READ, file, pos, count);
	if (ret)
		return ret;
	if (count > MAX_RW_COUNT)
		count =  MAX_RW_COUNT;
	//如果文件操作表中有read函数，则调用read读取文件
	if (file->f_op->read)
		ret = file->f_op->read(file, buf, count, pos);
    //如果文件操作表中有read_iter函数，则调用new_sync_read读取文件
	else if (file->f_op->read_iter)
		ret = new_sync_read(file, buf, count, pos);
	else
		ret = -EINVAL;
    
    //如果读取文件成功，进行更新访问标志,增加读取字节数
	if (ret > 0) {
		fsnotify_access(file);
		add_rchar(current, ret);
	}
	inc_syscr(current);//增加当前进程的系统调用计数
	return ret;
}
```

### read流程

使用perf生成的火焰图，描述了read的过程：

<img src=".\img\cp_11.png" style="zoom:67%;" />

## sys_write

此函数最终会通过ksys_write()来实现对文件的写入，ksys_write()的参数分别为`unsigned int fd, const char __user *buf, size_t count`

```c
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	return ksys_write(fd, buf, count);
}
```

### ksys_write

```c
ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);//通过fdget_pos来获取相应的文件描述符对应的结构
	ssize_t ret = -EBADF;
    
//相应的文件描述符对应的文件对象存在
	if (f.file) {
		loff_t pos, *ppos = file_ppos(f.file);//ppos指向当前文件的文件指针
		if (ppos) {
			pos = *ppos;//pos保存了文件的位移量
			ppos = &pos;
		}
		ret = vfs_write(f.file, buf, count, ppos);//这里主要通过vfs_write来实现对文件的写入
        
        // 如果写入成功且文件的偏移量有效，则更新文件的偏移量
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
        
        // 释放文件描述符
		fdput_pos(f);
	}

	return ret;
}
```

### vfs_write

```c
ssize_t vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;
//权限检查
	if (!(file->f_mode & FMODE_WRITE))//文件不允许写
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_WRITE))//文件是否可写
		return -EINVAL;
	if (unlikely(!access_ok(buf, count)))//是否可以访问buf
		return -EFAULT;
//调用 rw_verify_area 函数验证读取区域的有效性
	ret = rw_verify_area(WRITE, file, pos, count);
	if (ret)
		return ret;
	if (count > MAX_RW_COUNT)
		count =  MAX_RW_COUNT;
    
	file_start_write(file);
    
    //如果文件操作表中有write函数，则调用write实现对文件的写入
	if (file->f_op->write)
		ret = file->f_op->write(file, buf, count, pos);
     //如果文件操作表中有write_iter函数，则调用new_sync_write来实现对文件的写入
	else if (file->f_op->write_iter)
		ret = new_sync_write(file, buf, count, pos);
	else
		ret = -EINVAL;
      //如果读取文件成功，进行更新访问标志,增加读取字节数

	if (ret > 0) {
		fsnotify_modify(file);
		add_wchar(current, ret);
	}
	inc_syscw(current);//增加当前进程的系统调用计数
	file_end_write(file);
	return ret;
}
```

其中file_start_write以及file_end_write函数的调用流程如图所示，其原理就是信号量的pv操作

<img src=".\img\cp_12.png" style="zoom:67%;" />


### write流程

使用perf打印了write的执行流程，如下：

<img src=".\img\cp_13.png" alt="image-20231112221351419"  />



<img src=".\img\cp_14.png" style="zoom:67%;" />




## 三、心得体会

通过此次实现cp命令，在这块使用了之前学习过的一些知识，比如说查看系统内的文件类型，使用perf来查看函数的调用流程，使用open,read,write，close等系统调用。通过本次作业练习，使我把之前学过的东西进行回顾，越来越体会到使用perf工具进行分析代码的优势，是分析源码的一个好工具。
