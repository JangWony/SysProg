#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;
struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        // Implement read file operation
        struct packet *pckt;
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;
        unsigned long vpn1, vpn2, vpn3, vpn4, vpn5, vpo;

        pckt = (struct packet*) user_buffer;


        task = pid_task(find_get_pid(pckt->pid), PIDTYPE_PID);

        struct page* pg;
	pgd = pgd_offset(task -> mm, pckt -> vaddr);
        p4d = p4d_offset(pgd, pckt -> vaddr);
	pud = pud_offset(p4d, pckt -> vaddr);
	pmd = pmd_offset(pud, pckt -> vaddr);
	pte = pte_offset_map(pmd, pckt -> vaddr);
	pg = pte_page(*pte);
	pckt -> paddr = page_to_phys(pg);
	pte_unmap(pte);

        return length;
}

static const struct file_operations dbfs_fops = {
        // Mapping file operations with your functions
        .read = read_output,
};

static int __init dbfs_module_init(void)
{
        // Implement init module

        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // Fill in the arguments below
        output = debugfs_create_file("output", S_IWUSR, dir, NULL, &dbfs_fops);

	printk("dbfs_paddr module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module
        debugfs_remove_recursive(dir);
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
