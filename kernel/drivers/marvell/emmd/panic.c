/*
 *  Do flush operation when panic to save those stuff still in cache to mem
 *
 *  Copyright (C) 2013 Marvell International Ltd.
 *  Lei Wen <leiwen@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/reboot.h>
#include <linux/kexec.h>
#include <linux/kdebug.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <linux/kmsg_dump.h>
#include <linux/of.h>
#include <linux/cputype.h>
#include <linux/platform_data/mmp_sysset.h>
#include <linux/arm-coresight.h>
#include <linux/slab.h>
#include <marvell/emmd_rsv.h>
#ifdef CONFIG_REGDUMP
#include <linux/cputype.h>
#include <linux/regdump_ops.h>
#endif

#ifdef CONFIG_PXA_RAMDUMP
#include <linux/ramdump.h>
#endif

#define TAGS_NUM 8
#define VERSION 1
struct emmd_page *emmd_page;
static int *atf_base;
static int *atf_emmd_buffer;
#define ATF_PRINT_SIZE	96
#define ATF_DUMP_SIZE	SZ_256K

extern const unsigned long kallsyms_addresses[] __attribute__((weak));
extern const u8 kallsyms_names[] __attribute__((weak));
extern const unsigned long kallsyms_num_syms __attribute__((weak));
extern const u8 kallsyms_token_table[] __attribute__((weak));
extern const u16 kallsyms_token_index[] __attribute__((weak));
extern const unsigned long kallsyms_markers[] __attribute__((weak));

static DEFINE_RAW_SPINLOCK(panic_lock);
extern void arm_machine_flush_console(void);
extern void (*arm_pm_restart)(char str, const char *cmd);
#define PANIC_TIMER_STEP 100

static void dump_one_task_info(struct task_struct *tsk)
{
	char stat_array[3] = { 'R', 'S', 'D'};
	char stat_ch;

	stat_ch = tsk->state <= TASK_UNINTERRUPTIBLE ?
		stat_array[tsk->state] : '?';
	pr_info("%8d %8d %8d %16lld %c(%d) %4d    %p %s\n",
			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
			tsk->se.exec_start, stat_ch, (int)(tsk->state),
			task_cpu(tsk), tsk, tsk->comm);

	show_stack(tsk, NULL);
}

void dump_task_info(void)
{
	struct task_struct *g, *p;

	pr_info("\n");
	pr_info("current proc: %d %s\n", current->pid, current->comm);
	pr_info("-----------------------------------------------------------------------------------\n");
	pr_info("      pid     uTime    sTime    exec(ns)    stat   cpu   task_struct\n");
	pr_info("-----------------------------------------------------------------------------------\n");

	do_each_thread(g, p) {
		if (p->state == TASK_RUNNING ||
			p->state == TASK_UNINTERRUPTIBLE)
			dump_one_task_info(p);
	} while_each_thread(g, p);

	pr_info("-----------------------------------------------------------------------------------\n");
}

int emmd_enable = 1;
void set_emmd_indicator(void)
{
	if (emmd_page && emmd_enable)
		emmd_page->indicator = 0x454d4d44;
}

void clr_emmd_indicator(void)
{
	if (emmd_page)
		emmd_page->indicator = 0x0;
}

static void ramtag_setup(void)
{
	unsigned long sum = (unsigned long)kallsyms_addresses +
		(unsigned long)kallsyms_names +
		(unsigned long)kallsyms_num_syms +
		(unsigned long)kallsyms_token_table +
		(unsigned long)kallsyms_token_index +
		(unsigned long)kallsyms_markers;
	struct ram_tag_info *ram_tag = emmd_page->ram_tag;

	/* identify tag num */
	strcpy(ram_tag->name, "magic_id");
	ram_tag->data = TAGS_NUM;
	ram_tag++;

	strcpy(ram_tag->name, "version");
	ram_tag->data = VERSION;
	ram_tag++;

	strcpy(ram_tag->name, "check_sum");
	ram_tag->data = sum;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_addrs");
	ram_tag->data = (unsigned long)kallsyms_addresses;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_names");
	ram_tag->data = (unsigned long)kallsyms_names;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_num");
	ram_tag->data = (unsigned long)kallsyms_num_syms;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_token_tb");
	ram_tag->data = (unsigned long)kallsyms_token_table;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_token_id");
	ram_tag->data = (unsigned long)kallsyms_token_index;
	ram_tag++;

	strcpy(ram_tag->name, "kallsyms_markers");
	ram_tag->data = (unsigned long)kallsyms_markers;
	ram_tag++;
}

static int panic_flush(struct notifier_block *nb,
				unsigned long l, void *buf)
{
	int i;

	raw_spin_lock(&panic_lock);
	pr_emerg("EMMD: ready to perform memory dump\n");
/*	pr_info("Content of Firmware:\n");
	for (i = 0; i < ATF_PRINT_SIZE; i++) {
		printk(" %08x ", *(atf_base + i));
		if ((i + 1) % 8 == 0)
			printk("\n");
	}

	for (i = 0; i < ATF_DUMP_SIZE / 4; i++)
		atf_emmd_buffer[i] = *(atf_base + i); */

	for (i = 0; i < nr_cpu_ids; i++)
		coresight_dump_pcsr(i);
	set_emmd_indicator();
	ramtag_setup();

	kmsg_dump(KMSG_DUMP_PANIC);
	dump_task_info();

#ifdef CONFIG_PXA_RAMDUMP
	ramdump_panic();
#endif

#ifdef CONFIG_REGDUMP
	if (cpu_is_pxa1088() || cpu_is_pxa1L88())
		dump_reg_to_console();
#endif

	if (cpu_is_pxa1928())
		pxa1928_sysset_print();

	pr_emerg("EMMD: done\n");
	arm_machine_flush_console();

	flush_cache_all();
#ifdef CONFIG_ARM
	outer_flush_all();
#endif

	raw_spin_unlock(&panic_lock);

	return NOTIFY_DONE;
}

#if (defined CONFIG_PM)
static ssize_t panic_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t len)
{
	char _buf[80];

	if (strncmp(buf, "PID", 3) == 0) {
		snprintf(_buf, 80, "\nUser Space Panic:%s", buf);
		panic(_buf);
		goto OUT;
	}

	/*
	 * dump_style=1: force USB dump when panic;
	 * Or it will be SD dump default.
	 */
	if (strncmp(buf, "USB", 3) == 0) {
		emmd_page->dump_style = 0x1;
		goto OUT;
	}

	pr_warn("Not valid value!!!\n");
OUT:
	return len;
}

static struct kobj_attribute panic_attr = {
	.attr	= {
		.name = __stringify(panic),
		.mode = 0644,
	},
	.store	= panic_store,
};

static struct attribute *g[] = {
	&panic_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

#endif /* CONFIG_PM */

static int _reboot_notifier_evt(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	clr_emmd_indicator();
	return NOTIFY_DONE;
}

static struct notifier_block _reboot_notifier = {
	.notifier_call = _reboot_notifier_evt,
};

static struct notifier_block nb_panic_block = {
	.notifier_call = panic_flush,
};

static int __init pxa_panic_init(void)
{
	struct device_node *np;
	void __iomem *fw_base;
#if (defined CONFIG_PM)
	if (sysfs_create_group(power_kobj, &attr_group))
		return -1;
#endif
	if (crashk_res.end && crashk_res.start) {
		emmd_page = phys_to_virt(crashk_res.start);
		memset((void *)emmd_page, 0, sizeof(*emmd_page));
	}

	panic_on_oops = 1;

	np = of_find_node_by_name(NULL, "pxa_panic_set_emmd_indicator");
	if (np) {
		set_emmd_indicator();
		register_reboot_notifier(&_reboot_notifier);
	}

	fw_base = ioremap(0x0, ATF_DUMP_SIZE);
	if (!fw_base) {
		pr_err("error to ioremap Firmware base address\n");
		return -ENOMEM;
	}
	atf_base = fw_base;

	atf_emmd_buffer = (int *)__get_free_pages(GFP_KERNEL, 6);
	if (!atf_emmd_buffer) {
		pr_err("error to alloc buffer for atf emmd buffer!\n");
		return -ENOMEM;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	return 0;
}

core_initcall_sync(pxa_panic_init);
