#include <linux/kernel.h>
#include <linux/linkage.h>

asmlinkage int sys_print_str(char str[100]){
	printk("%s", str);
	return 0;
}
