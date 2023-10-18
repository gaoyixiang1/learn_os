/*
    内核双链表信息打印

*/
#include<linux/module.h>
#include <linux/kernel.h>
// #include <linux/init.h>
#include<linux/list.h>
#include<linux/slab.h>
#define N 100
#define M 10
//链表的结构定义
struct my_list{
    int data;
    struct list_head list;
};

//链表的头节点
struct my_list listHead;

static int __init doublelist_init(void)
{
    printk("<1>init\n");
    
    struct list_head *p;//遍历用
    struct my_list *q;
    struct my_list *node; //声明节点的时候所用
    int i;
    INIT_LIST_HEAD(&listHead.list);//初始化头节点

    for(int i=0;i<N;i++){
        node = (struct my_list *)kmalloc(sizeof(struct my_list),GFP_KERNEL);//申请空间
        node->data = i+1;
        list_add_tail(&node->list,&listHead.list);
        printk("<1>%d has added\n",i+1);
    }
    return 0;
}
static  void __exit doublelist_exit(void)
{
    //删除节点
    printk("<1>GoodBye!\n");
    struct list_head *pos,*n;
    struct my_list *p,*q;
    int i;

    i=1;
    list_for_each_safe(pos,n,&listHead.list){
        list_del(pos);
        p=list_entry(pos, struct my_list,list);
        kfree(p);
        printk("<1>%d is delete\n",i);
        if(i<M) i++;
        else break;
    }
    i=1;
    printk("<1>list is starting\n");
    list_for_each(pos,&listHead.list){
            q=list_entry(pos,struct my_list,list);
            printk("<1>%d data is %d\n",i,q->data);
            i++;
    }
}

module_init(doublelist_init);
module_exit(doublelist_exit);
MODULE_LICENSE("GPL");