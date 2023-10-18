/*
    模块内以传参的形式传入a,b，并计算a+b。要求在Makefile文件中，至少有2个以上的文件
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
extern int add(int a,int b);
static int arr[2];
module_param_array(arr,int,NULL,0644);
static int __init my_test_init(void)
{
    int sum =add(arr[1],arr[0]);
    printk("sum=%d\n",sum);
    return 0;
}
static void __exit my_test_exit(void)
{
    printk("goodbye\n");
}
module_init(my_test_init);
module_exit(my_test_exit);
MODULE_LICENSE("GPL");