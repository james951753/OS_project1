#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage int sys_my_add(int a, int b)
{
    printk(“my_add is invoked!\n");
    return a + b;
}
