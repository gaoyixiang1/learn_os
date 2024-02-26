# 打开文件源代码分析以及打印相关信息

```c
结合自己机子上内核版本，对open()系统调用的源代码进行分析：
1.给出源码分析过程，流程图，心得体会，提出3个问题并回答（12分）
2. 编写用户态代码，并打开一个文件，编写内核模块，从task_struct结构开始，打印该进程文件系统、文件等相关信息，通过这个过程，说明自己对文件系统相关数据结构的真切理解，打印字段不少于10个，有自己的想法更好（10分）
```

# 一、6.2 版本open()系统调用源码

test.c内容为

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> // 包含头文件 <unistd.h>
int main() {
    char *file_path = "test.txt";
    int fd;
    while(1){//通过while循环打开关闭文件，便于perf监测到open()函数；
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error in opening file");
        return -1;
    } else {
     printf("File opened successfully with file descriptor: %d\n",fd);
    // 在此处可以执行其他操作，比如读取文件内容或者进行其他处理。
     close(fd); // 关闭文件
    }
        printf("PID==>%d\n",getpid());//获得该进程pid，便于使用perf跟踪该进程；
    }
    return 0;
}
```





在学习open源码前，使用perf搞了一个函数流程大概的框架，但不是很完整，又想起来上次跑了刘冰师兄的项目，于是就使用Stack_Analyser进行分析

![image-20231107212501161](.\img\open_1.png)

![image-20231107213038885](.\img\open_2.png)

生成的火焰图如图所示：

![image-20231107213337727](.\img\open_3.png)

通过这个图清楚直观的看到函数的调用关系，为后面分析open源码做准备。

通过李治军老师系统调用的实验一节，明白open系统调用，在内核里面应该会有sys_open函数来实现。关于[李治军老师的系统调用](https://blog.csdn.net/qq_43573047/article/details/133768813?spm=1001.2014.3001.5501)可以看看这篇博客。于是在6.2版本搜索了sys_open，代码如下：

## 1.1 sys_open

```assembly
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile())
		flags |= O_LARGEFILE;
	return do_sys_open(AT_FDCWD, filename, flags, mode);
}
```



关于宏SYSCALL_DEFINE3定义在`linux/v6.2/source/include/linux/syscalls.h`中

```assembly
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define SYSCALL_DEFINEx(x, sname, ...)				\
	SYSCALL_METADATA(sname, x, __VA_ARGS__)			\
	__SYSCALL_DEFINEx(x, sname, __VA_ARGS__)
```



展开SYSCALL_DEFINE3如下：

```assembly
//转化后结果
SYSCALL_METADATA(_open, 3, const char __user *, filename, int, flags, umode_t, mode)
__SYSCALL_DEFINEx(3, _open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile())
		flags |= O_LARGEFILE;

	return do_sys_open(AT_FDCWD, filename, flags, mode);
	/*AT_FDCWD Special value used to indicate openat should use the current working directory. */
	//AT_FDCWD 是一个特殊的标志值，通常在文件操作中用于指示系统应该使用当前工作目录（Current Working //Directory，CWD）作为操作的参考目录。这个标志值通常与诸如 openat 等文件操作系统调用一起使用。
}
```

本着分析代码抓住主干的原则，对于后面`SYSCALL_METADATA`以及`__SYSCALL_DEFINEx`的宏就不详细展开，后面这里是调用了`do_sys_open`函数

## 1.2 do_sys_open

调用sys_open函数实际上是调用了do_sys_open函数来实现打开文件这一功能。下面开始分析do_sys_open函数：

```assembly
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_how how = build_open_how(flags, mode);//构造一个open_how结构体
	return do_sys_openat2(dfd, filename, &how);//主要执行do_sys_openat2打开文件
}
```

```c
inline struct open_how build_open_how(int flags, umode_t mode)
{
	struct open_how how = {
		.flags = flags & VALID_OPEN_FLAGS,//保留有效打开标志位
		.mode = mode & S_IALLUGO,//保留有效的文件模式
	};

	/* O_PATH beats everything else. */
	if (how.flags & O_PATH)//检查是否包含O_PATH标志
		how.flags &= O_PATH_FLAGS;//如果 flags 中包含 O_PATH 标志，那么将flags 与 O_PATH_FLAGS 进行位操作，以确保只保留 O_PATH_FLAGS 中的标志位
	/* Modes should only be set for create-like flags. */
	if (!WILL_CREATE(how.flags))//检查是否需要创建文件
		how.mode = 0;
	return how;//返回了open_how结构体类型的how,规定了打开文件的方式等
}
```

## 1.3 do_sys_openat2

实际是通过调用do_sys_openat2来打开文件，接下来分析此函数。

```assembly
static long do_sys_openat2(int dfd, const char __user *filename,
			   struct open_how *how)
{
	struct open_flags op;
	int fd = build_open_flags(how, &op);//创建文件打开标志，返回一个文件描述符fd
	struct filename *tmp;

	if (fd) 
		return fd;//打开文件成功，则返回fd
		
	//打开文件不成功的话，执行下列代码
	
	tmp = getname(filename);//通过 getname 函数获取文件名的 filename 结构体
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);//如果获取文件名的 filename 结构体失败，则返回错误码
	fd = get_unused_fd_flags(how->flags);//获取一个未使用的文件描述符fd
	
	//成功获取未使用的描述符fd
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);//调用do_filp_open来打开tmp文件
		//打开文件失败
		if (IS_ERR(f)) {
			put_unused_fd(fd);//把当前的fd放入到未使用的fd中
			fd = PTR_ERR(f);
		} else {
		//打开文件成功
			fsnotify_open(f);
			fd_install(fd, f);//将文件和文件描述符关联起来，即fd被使用
		}
	}
	putname(tmp);//释放文件名的 filename 结构体
	return fd;
}
```

从sys_open到do_sys_open在到do_sys_openat2的流程图如图所示：

<img src=".\img\open_4.png" alt="image-20231107164750209" style="zoom: 67%;" />



## 1.4 nameidata结构体

在学习do_file_open之前 ，先了解一下nameidata结构，这样在后面的学习过程中就不会太过于迷茫。nameidata结构是路径查找过程中的核心数据结构，在每一级目录项查找过程中，它向查找函数输入参数，并且保存本次查找的结果

```c
struct nameidata {
	struct path	path;//该字段用于保存当前目录项
	struct qstr	last;//表示当前目录项的名称
	struct path	root;//表示根目录
	struct inode	*inode; /* 表示当前目录项对应的inode，它的取值来自于path.dentry.d_inode */
	unsigned int	flags, state;
	unsigned	seq, next_seq, m_seq, r_seq;
	int		last_type;//表示当前目录项的类型

	unsigned	depth;//表示符号链接当前的嵌套级别
	int		total_link_count;//链接数
	...
} __randomize_layout;
```

## 1.5 do_filp_open

`do_filp_open` 函数的主要功能是尝试打开文件，首先在 RCU 模式下进行尝试，如果失败则在传统的 ref-walk 模式下再次尝试，并最终返回打开文件的结果或错误码。在此函数中主要通过path_openat来完成打开文件

<img src=".\img\open_5.png" alt="image-20231107174038659" style="zoom:67%;" />

```c
struct file *do_filp_open(int dfd, struct filename *pathname,
		const struct open_flags *op)
{
	struct nameidata nd;
	int flags = op->lookup_flags;//flags=打开标志位的lookup_flags(包含有关文件查找和打开的标志位）
	struct file *filp;

	set_nameidata(&nd, dfd, pathname, NULL);//初始化nd结构体，准备打开文件
	filp = path_openat(&nd, op, flags | LOOKUP_RCU);//在RCU(rcu-walk)模式下使用path_openat来打开文件
	if (unlikely(filp == ERR_PTR(-ECHILD)))//如果在RCU(rcu-walk)模式打开文件失败，错误码-ECHILD
		filp = path_openat(&nd, op, flags);//在传统（ref-walk）模式下使用path_openat来打开文件
	if (unlikely(filp == ERR_PTR(-ESTALE)))//如果在传统（ref-walk）模式下打开文件失败，错误码-ESTALE
		filp = path_openat(&nd, op, flags | LOOKUP_REVAL);//路径查找的重新验证,这种情况一般出现在分布式系统
	restore_nameidata();//清理nameidata结构体
	return filp;//成功返回文件结构体或错误码
}
```

## 1.6 path_openat

path_openat的执行过程如下，最后主要是通过do_open函数进行打开文件操作的

<img src=".\img\open_6.png" alt="image-20231107194941839"  />

```c
static struct file *path_openat(struct nameidata *nd,
			const struct open_flags *op, unsigned flags)
{
	struct file *file;
	int error;

	file = alloc_empty_file(op->open_flag, current_cred());//申请分配一个新的文件结构体
	if (IS_ERR(file))
		return file;//分配失败，返回错误码
		
		//分配成功
		
//如果file的flags中 __O_TMPFILE存在，说明file是临时文件
	if (unlikely(file->f_flags & __O_TMPFILE)) {
		error = do_tmpfile(nd, flags, op, file);//打开临时文件，调用do_tmpfile
		
		//如果file的flags中  O_PATH存在，表示以路径方式打开文件，通常用于获取文件的元数据而不执行实际的读写操作。
	} else if (unlikely(file->f_flags & O_PATH)) {
		error = do_o_path(nd, flags, file);//打开一个路径，调用do_o_path
		
		//file的flags中 __O_TMPFILE和O_PATH都不存在
		
	} else {
	//打开一个普通文件
	
		const char *s = path_init(nd, flags);//用于设置路径搜寻的起始位置
		
		//查找路径，对各目录项逐级遍历，成功遍历并且下一个目录项不为空，就一直重复此操作
		while (!(error = link_path_walk(s, nd)) &&
		       (s = open_last_lookups(nd, file, op)) != NULL)
			;
		if (!error)//路径查找成功
			error = do_open(nd, file, op);//调用do_open来打开文件
		terminate_walk(nd);//终止路径查找
	}
	//检查文件是否打开成功
	if (likely(!error)) {
		if (likely(file->f_mode & FMODE_OPENED))//成功打开文件，检查f_mode存在 FMODE_OPENED标志位，如果存在此标志位说明文件已经处于已打开的状态
			return file;//如果文件已打开，则返回文件指针
		WARN_ON(1);
		error = -EINVAL;
	}
	//打开文件失败，调用fput释放文件结构
	fput(file);
	//判断错误码情况，进行错误处理
	if (error == -EOPENSTALE) {
		if (flags & LOOKUP_RCU)//如果在RCU模式下打开文件失败
			error = -ECHILD;
		else
			error = -ESTALE;//否则是在普通模式下打开文件失败
	}
	return ERR_PTR(error);
}
```

## 1.7 do_open

do_open函数的分析过程如图，其实最后是调用了vfs_open函数来打开文件的

<img src=".\img\open_7.png" alt="image-20231107195050808" style="zoom:67%;" />

```c
/*
 * Handle the last step of open()//open系统调用的最后一步
 */
static int do_open(struct nameidata *nd,
		   struct file *file, const struct open_flags *op)
{
	struct user_namespace *mnt_userns;//用于存储用户命名空间的指针
	int open_flag = op->open_flag;//存储打开文件的标志
	bool do_truncate;//截断标志位
	int acc_mode; //存储文件访问模式
	int error;
	
/*检查文件是否已经打开或已创建*/
/*如果f_mode不存在FMODE_OPENED或FMODE_CREATED，则表示文件没有打开也没有创建*/
	if (!(file->f_mode & (FMODE_OPENED | FMODE_CREATED))) {
		error = complete_walk(nd);//调用 complete_walk()完成路径查找
		if (error)
			return error;
	}
	
	
	/*文件没有创建*/
	if (!(file->f_mode & FMODE_CREATED))
		audit_inode(nd->name, nd->path.dentry, 0);//记录日志，并跟踪打开的文件
	mnt_userns = mnt_user_ns(nd->path.mnt);//获取与挂载点相关的命名空间
	//判断是否在flag中包含了O_CREAT
	if (open_flag & O_CREAT) {
	/*如果设置O_EXCL且O_CREAT,则要求文件不存在
	并且文件创建失败*/
		if ((open_flag & O_EXCL) && !(file->f_mode & FMODE_CREATED))
			return -EEXIST;//返回-EEXIST错误码
		if (d_is_dir(nd->path.dentry))//如果文件是目录
			return -EISDIR;//是的话返回-EISDIR
			
		error = may_create_in_sticky(mnt_userns, nd,
					     d_backing_inode(nd->path.dentry));//在具有粘性位的情况下创建文件
		if (unlikely(error))
			return error;
	}
	//如果路径查找标志指示要查找的是目录，但是该目录无法查找，返回非目录错误
	if ((nd->flags & LOOKUP_DIRECTORY) && !d_can_lookup(nd->path.dentry))
		return -ENOTDIR;

	do_truncate = false;//不可截断
	acc_mode = op->acc_mode;//获取文件的访问方式
	//f_mode是否包含FMODE_CREATED
	if (file->f_mode & FMODE_CREATED) {
	//包含的话代表是文件创建了，则不用检查写的权限
		/* Don't check for write permission, don't truncate */
		open_flag &= ~O_TRUNC;//将O_TRUNC设置为0
		acc_mode = 0;//访问模式设置为0
		//如果flag包含O_TRUNC且目标路径是否是regular file，常规文件
	} else if (d_is_reg(nd->path.dentry) && open_flag & O_TRUNC) {
		error = mnt_want_write(nd->path.mnt);//调用mnt_want_write来获取写权限
		if (error)
			return error;//获取写权限失败
		do_truncate = true;//设置可截断
	}
	/*调用may_open来打开文件并获取相应权限*/
	error = may_open(mnt_userns, &nd->path, acc_mode, open_flag);
	//如果获取成功但是f_mode不包含FMODE_OPENED标志位，也就是说未打开
	if (!error && !(file->f_mode & FMODE_OPENED))
		error = vfs_open(&nd->path, file);//调用vfs_open来打开文件
	if (!error)//如果打开成功
		error = ima_file_check(file, op->acc_mode);//调用ima_file_check来检查文件的完整性
	if (!error && do_truncate)//如果成功且需要do_truncate截断文件
		error = handle_truncate(mnt_userns, file);//调用handle_truncate来截断文件
	if (unlikely(error > 0)) {
		WARN_ON(1);
		error = -EINVAL;
	}
	if (do_truncate)
		mnt_drop_write(nd->path.mnt);//如果需要截断文件，释放挂载点的写权限
	return error;
}
```

## 1.8 file->f_flags&&file->f_mode取值及其含义

由于在分析源代码过程中会经常遇到f_flags及f_mode。总觉得两个差不多，不太清楚这俩具体的区别，于是问了chatgpt。

<img src=".\img\open_8.png" alt="image-20231106102707349" style="zoom:67%;" />

<img src=".\img\open_9.png" alt="image-20231106104229519" style="zoom:67%;" />



## 1.9 vfs_open

很显然在vfs_open中，将文件结构体中的f_path字段设置为传入的路径结构体path，最终调用了do_dentry_open来执行打开文件的操作

```c
int vfs_open(const struct path *path, struct file *file)
{
	file->f_path = *path;//将文件结构体中的f_path字段设置为传入的路径结构体path
	return do_dentry_open(file, d_backing_inode(path->dentry), NULL);/// 调用do_dentry_open函数来实际执行文件打开操作
}

```



## 1.10 do_dentry_open

```c
static int do_dentry_open(struct file *f,
			  struct inode *inode,
			  int (*open)(struct inode *, struct file *))
{
	static const struct file_operations empty_fops = {};//初始化空的file_operations结构体
	int error;

	path_get(&f->f_path);//通过path_get获取文件对象 f 关联的路径信息的引用，以确保在使用过程中路径信息不被释放。
    //设置文件对象的各种属性
    
    //file与inode关联
	f->f_inode = inode;//设置文件索引节点
	f->f_mapping = inode->i_mapping;//设置文件的映射
	f->f_wb_err = filemap_sample_wb_err(f->f_mapping);
	f->f_sb_err = file_sample_sb_err(f);//超级块错误
    
    //如果file的flags中O_PATH存在，表示以路径方式打开文件，通常用于获取文件的元数据而不执行实际的读写操作
	if (unlikely(f->f_flags & O_PATH)) {
		f->f_mode = FMODE_PATH | FMODE_OPENED;
		f->f_op = &empty_fops;//文件的操作指针为一个空的文件操作结构
		return 0;
	}
	//如果文件只有读权限，增加inode的读引用计数
	if ((f->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ) {
		i_readcount_inc(inode);//否则文件可写且不是特殊文件（pip等）
	} else if (f->f_mode & FMODE_WRITE && !special_file(inode->i_mode)) {
		error = get_write_access(inode);//获取对inode对应文件的写权限
		if (unlikely(error))
			goto cleanup_file;//获取写权限失败
		error = __mnt_want_write(f->f_path.mnt);//获取写权限成功，调用 __mnt_want_write来获取挂载点的写权限
		if (unlikely(error)) {
			put_write_access(inode);//如果没有获取到对挂载点的写权限，则收回对inode对应的文件的写权限
			goto cleanup_file;
		}
        //给文件加入写权限
		f->f_mode |= FMODE_WRITER;
	}

	/* POSIX.1-2008/SUSv4 Section XSI 2.9.7 */
    //如果索引节点的访问权限是否是常规文件或目录
	if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode))
		f->f_mode |= FMODE_ATOMIC_POS;//设置原子定位模式
	
	f->f_op = fops_get(inode->i_fop);//通过fops_get获取inode的索引节点操作，并让文件操作指针指向它
	if (WARN_ON(!f->f_op)) {
		error = -ENODEV;
		goto cleanup_all;
	}

	error = security_file_open(f);//检查打开文件安全性
	if (error)
		goto cleanup_all;
	/*检查并处理文件锁*/
	error = break_lease(locks_inode(f), f->f_flags);
	if (error)
		goto cleanup_all;

	/* normally all 3 are set; ->open() can clear them if needed */
	f->f_mode |= FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
	if (!open)//如果open函数不存在，则使用f_op->open
		open = f->f_op->open;
	if (open) {
		error = open(inode, f);//调用open函数打开文件
		if (error)
			goto cleanup_all;
	}
	f->f_mode |= FMODE_OPENED;//文件打开正常
	if ((f->f_mode & FMODE_READ) &&
	     likely(f->f_op->read || f->f_op->read_iter))
		f->f_mode |= FMODE_CAN_READ;//设置文件为可读
	if ((f->f_mode & FMODE_WRITE) &&
	     likely(f->f_op->write || f->f_op->write_iter))
		f->f_mode |= FMODE_CAN_WRITE;//设置文件为可写
	if ((f->f_mode & FMODE_LSEEK) && !f->f_op->llseek)
		f->f_mode &= ~FMODE_LSEEK;//文件不支持定位操作。
	if (f->f_mapping->a_ops && f->f_mapping->a_ops->direct_IO)
		f->f_mode |= FMODE_CAN_ODIRECT;

	f->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);
	f->f_iocb_flags = iocb_flags(f);

	file_ra_state_init(&f->f_ra, f->f_mapping->host->i_mapping);

	if ((f->f_flags & O_DIRECT) && !(f->f_mode & FMODE_CAN_ODIRECT))
		return -EINVAL;

	/*
	 * XXX: Huge page cache doesn't support writing yet. Drop all page
	 * cache for this file before processing writes.
	 */
    //如果文件打开的时候是可写状态
	if (f->f_mode & FMODE_WRITE) {
		/*
		 * Paired with smp_mb() in collapse_file() to ensure nr_thps
		 * is up to date and the update to i_writecount by
		 * get_write_access() is visible. Ensures subsequent insertion
		 * of THPs into the page cache will fail.
		 */
        /*这是一个内存屏障，用于确保下面的操作有序进行*/
		smp_mb();
        /*如果有巨大页缓存，则需要清除这些缓存*/
		if (filemap_nr_thps(inode->i_mapping)) {
			struct address_space *mapping = inode->i_mapping;

			filemap_invalidate_lock(inode->i_mapping);
			/*
			 * unmap_mapping_range just need to be called once
			 * here, because the private pages is not need to be
			 * unmapped mapping (e.g. data segment of dynamic
			 * shared libraries here).
			 */
			unmap_mapping_range(mapping, 0, 0, 0);
			truncate_inode_pages(mapping, 0);
			filemap_invalidate_unlock(inode->i_mapping);
		}
	}

	return 0;

cleanup_all:
	if (WARN_ON_ONCE(error > 0))
		error = -EINVAL;
	fops_put(f->f_op);//释放文件操作指针
	put_file_access(f);
cleanup_file:
	path_put(&f->f_path);//减少路径结构f->f_path的引用计数
	f->f_path.mnt = NULL;
	f->f_path.dentry = NULL;
	f->f_inode = NULL;
	return error;
}
```

对于do_dentry_open函数的分析总体来说难度大多了，在这里我只弄清楚了这个函数的大概执行流程，如图所示，至于do_dentry后面都执行了哪些函数，具体查看上面的火焰图。

![image-20231107203643641](.\img\open_10.png)

## 1.11 心得体会

​	通过学习并分析open的源码，使我深刻的理解了打开文件的详细过程，对于之前学习的系统调用以及函数流程分析再一次应用，并且我发现使用流程图画出每个函数的调用过程，非常有助于理解函数的功能，以后在分析源码的时候可以通过画流程图来理清思路，有一点成就感，但是我在分析的过程中还是对于一些函数有点不太明白，比如1.10 do_dentry_open，下来我将通过相关书籍以及查阅网上相关资料来弥补这一缺点。

# 二、打印信息

通过翻阅《深入理解Linux内核》一书，重新看了进程描述符部分内容，书上给了很详细的分类，如图所示

<img src=".\img\open_11.png" alt="image-20231108082048128" style="zoom:67%;" />

于是就这次就着重分析了task_struct内部的以下成员：

```c
	/* Filesystem information: */
	struct fs_struct		*fs;

	/* Open file information: */
	struct files_struct		*files;
```

展开这些数据结构：

```c
struct fs_struct {
	int users;
	spinlock_t lock;
	seqcount_spinlock_t seq;
	int umask;
	int in_exec;
	struct path root, pwd;//表示进程的根文件系统路径及当前工作目录路径
} __randomize_layout;

//用户打开文件表
struct files_struct {
  /*
   * read mostly part
   */
	atomic_t count;//共享该表的进程数
	bool resize_in_progress;
	wait_queue_head_t resize_wait;

	struct fdtable __rcu *fdt;
	struct fdtable fdtab;//实际的文件描述符表数据结构，存储了进程的文件描述符信息。
  /*
   * written part on a separate cache line in SMP
   */
	spinlock_t file_lock ____cacheline_aligned_in_smp;
	unsigned int next_fd;//当前描述符的下一个可用的描述符，一般是已分配的描述符+1
	unsigned long close_on_exec_init[1];//指向执行exec()时需要关闭的文件描述符
	unsigned long open_fds_init[1];
	unsigned long full_fds_bits_init[1];
	struct file __rcu * fd_array[NR_OPEN_DEFAULT];//文件对象指针的初始化数组
};

//进程文件描述符表
struct fdtable {
	unsigned int max_fds;//当前文件对象的最大数
	struct file __rcu **fd;  //表示文件描述符表中的文件描述符    /* current fd array */
	unsigned long *close_on_exec;
	unsigned long *open_fds;
	unsigned long *full_fds_bits;
	struct rcu_head rcu;
};


struct file {
	union {
		struct llist_node	f_llist;
		struct rcu_head 	f_rcuhead;
		unsigned int 		f_iocb_flags;
	};
	struct path		f_path;//表示文件的路径
	struct inode		*f_inode;	/*一个指向与文件相关联的inode结构的指针 */
	const struct file_operations	*f_op;//个指向文件操作函数的指针

	/*
	 * Protects f_ep, f_flags.
	 * Must not be taken from IRQ context.
	 */
	spinlock_t		f_lock;
	atomic_long_t		f_count;//表示文件的引用计数
	unsigned int 		f_flags;//表示文件的标志
	fmode_t			f_mode;//文件打开模式
	...
} __randomize_layout
  __attribute__((aligned(4)));	/* lest something weird decides that 2 is OK */




struct inode {
	umode_t			i_mode;
	unsigned short		i_opflags;
	kuid_t			i_uid;//使用者id
	kgid_t			i_gid;
	unsigned int		i_flags;


	const struct inode_operations	*i_op;//节点操作
	struct super_block	*i_sb;//相关超级块
	struct address_space	*i_mapping;



	/* Stat data, not accessed from path walking */
	unsigned long		i_ino;//节点号
	/*
	 * Filesystems may only read i_nlink directly.  They shall use the
	 * following functions for modification:
	 *
	 *    (set|clear|inc|drop)_nlink
	 *    inode_(inc|dec)_link_count
	 */
	union {
		const unsigned int i_nlink;
		unsigned int __i_nlink;
	};
	dev_t			i_rdev;
	loff_t			i_size;
	struct timespec64	i_atime;
	struct timespec64	i_mtime;
	struct timespec64	i_ctime;
	spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
	unsigned short          i_bytes;
	u8			i_blkbits;
	u8			i_write_hint;
	blkcnt_t		i_blocks;

	/* Misc */
	unsigned long		i_state;
	struct rw_semaphore	i_rwsem;

	unsigned long		dirtied_when;	/* jiffies of first dirtying */
	unsigned long		dirtied_time_when;

	struct hlist_node	i_hash;
	struct list_head	i_io_list;	/* backing dev IO list */

	struct list_head	i_lru;		/* inode LRU list */
	struct list_head	i_sb_list;
	struct list_head	i_wb_list;	/* backing dev writeback list */
	union {
		struct hlist_head	i_dentry;
		struct rcu_head		i_rcu;
	};
	atomic64_t		i_version;
	atomic64_t		i_sequence; /* see futex */
	atomic_t		i_count;
	atomic_t		i_dio_count;
	atomic_t		i_writecount;

	union {
		const struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
		void (*free_inode)(struct inode *);
	};
	struct file_lock_context	*i_flctx;
	struct address_space	i_data;
	struct list_head	i_devices;
	union {
		struct pipe_inode_info	*i_pipe;
		struct cdev		*i_cdev;
		char			*i_link;
		unsigned		i_dir_seq;
	};

	__u32			i_generation;



	void			*i_private; /* fs or device private pointer */
} __randomize_layout;

struct fd {
	struct file *file;
	unsigned int flags;
};
```





下图为与进程相关数据结构之间的联系

![image-20231108084323571](.\img\open_12.png)

### 代码实现

```c
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

```

### 运行结果

![image-20231114162626996](.\img\open_13.png)

