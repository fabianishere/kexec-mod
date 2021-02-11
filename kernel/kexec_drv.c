#define MODULE_NAME "kexec_mod"
#define pr_fmt(fmt) MODULE_NAME ": " fmt

#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/reboot.h>
#include <linux/sysfs.h>
#include <uapi/linux/stat.h>

#include "kexec_compat.h"
#include "kexec.h"
#include "idmap.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fabian Mastenbroek <mail.fabianm@gmail.com>");
MODULE_DESCRIPTION("Kexec backport as Kernel Module");
MODULE_VERSION("1.0");

static int disable_el2_boot;
module_param(disable_el2_boot, int, 0);
MODULE_PARM_DESC(disable_el2_boot,
		 "Do not use the hypervisor (EL2) to perform soft restart");

static ssize_t kexecmod_loaded_show(struct kobject *kobj,
		  		    struct kobj_attribute *attr, char *buf)
{
	extern struct kimage *kexec_image;
	return sprintf(buf, "%d\n", !!kexec_image);
}

static struct kobj_attribute kexec_loaded_attr = __ATTR(kexec_loaded, S_IRUGO, kexecmod_loaded_show, NULL);

static long kexecmod_ioctl(struct file *file, unsigned req, unsigned long arg)
{
	struct {
		unsigned long entry;
		unsigned long nr_segs;
		struct kexec_segment *segs;
		unsigned long flags;
	} ap;
	switch (req) {
	case LINUX_REBOOT_CMD_KEXEC - 1:
		if (copy_from_user(&ap, (void*)arg, sizeof ap))
			return -EFAULT;
		return sys_kexec_load(ap.entry, ap.nr_segs, ap.segs, ap.flags);
	case LINUX_REBOOT_CMD_KEXEC:
		return kernel_kexec();
	}
	return -EINVAL;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = kexecmod_ioctl,
};

int kexec_maj;
struct class *kexec_class;
struct device *kexec_device;
dev_t kexec_dev;

static int __init
kexecmod_init(void)
{
	int err;
	
	pr_info("Installing Kexec functionalitiy.\n");

	/* Load compatibility layer */	
	if ((err = kexec_compat_load(!disable_el2_boot)) != 0) {
		pr_err("Failed to load: %d\n", err);
		return err;
	}

	/* Build identity map for MMU */
	kexec_idmap_setup();

	/* Register character device at /dev/kexec */
	kexec_maj = register_chrdev(0, "kexec", &fops);
	if (kexec_maj < 0)
		return kexec_maj;
	kexec_class = class_create(THIS_MODULE, "kexec");
	if (IS_ERR(kexec_class))
		return PTR_ERR(kexec_class);
	kexec_dev = MKDEV(kexec_maj, 0);
	kexec_device = device_create(kexec_class, 0, kexec_dev, 0, "kexec");
	if (IS_ERR(kexec_device))
		return PTR_ERR(kexec_device);

	/* Register sysfs object */
	err = sysfs_create_file(kernel_kobj, &(kexec_loaded_attr.attr));

	return 0;
}

module_init(kexecmod_init)

static void __exit
kexecmod_exit(void)
{
	pr_info("Stopping...\n");
	/* Destroy character device */
	device_destroy(kexec_class, kexec_dev);
	class_destroy(kexec_class);
	unregister_chrdev(kexec_maj, "kexec");

	/* Remove sysfs object */
	sysfs_remove_file(kernel_kobj, &(kexec_loaded_attr.attr));
}

module_exit(kexecmod_exit);	
