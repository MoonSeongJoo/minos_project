#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define PROC_NAME "hasee_intervene"

/*
 * These values must match Minos include/virt/hypercall.h
 *
 * HVC_CALL_BASE       = 0xc0000000
 * HVC_TYPE_VM0        = 0x8
 * HVC_TYPE_MISC       = 0x9
 *
 * NOTE:
 * HVC_TYPE_VM0 is Minos VM management HVC group name.
 * It is not the DTS vmid=0.
 */
#define HVC_CALL_BASE       0xc0000000UL
#define HVC_TYPE_VM0        0x08UL
#define HVC_TYPE_MISC       0x09UL

#define HVC_CALL_NUM(t, n)  (HVC_CALL_BASE + ((t) << 24) + (n))
#define HVC_VM0_FN(n)       HVC_CALL_NUM(HVC_TYPE_VM0, n)
#define HVC_MISC_FN(n)      HVC_CALL_NUM(HVC_TYPE_MISC, n)

/* Existing Minos VM management hypercalls */
#define HVC_VM_RESTART      HVC_VM0_FN(2)
#define HVC_VM_POWER_UP     HVC_VM0_FN(3)
#define HVC_VM_POWER_DOWN   HVC_VM0_FN(4)

/* Existing misc hypercalls */
#define HVC_GET_VMID        HVC_MISC_FN(0)
#define HVC_GET_VM_CAP      HVC_MISC_FN(2)

/* Capability bits from Minos */
#define VM_CAP_HOST         (1 << 0)
#define VM_CAP_NATIVE       (1 << 1)

static struct proc_dir_entry *hasee_proc;

static long hvc_call3(unsigned long fid,
                      unsigned long a1,
                      unsigned long a2,
                      unsigned long a3)
{
    register unsigned long x0 asm("x0") = fid;
    register unsigned long x1 asm("x1") = a1;
    register unsigned long x2 asm("x2") = a2;
    register unsigned long x3 asm("x3") = a3;

    asm volatile("hvc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3)
                 : "memory");

    return (long)x0;
}

static long hvc_call0(unsigned long fid)
{
    return hvc_call3(fid, 0, 0, 0);
}

static void hasee_probe_current_vm(void)
{
    long vmid;
    long cap;

    vmid = hvc_call0(HVC_GET_VMID);
    cap = hvc_call0(HVC_GET_VM_CAP);

    pr_info("hasee_vm_ctl: current vmid=%ld cap=0x%lx host=%s native=%s\n",
            vmid,
            cap,
            (cap & VM_CAP_HOST) ? "yes" : "no",
            (cap & VM_CAP_NATIVE) ? "yes" : "no");
}

static long hasee_request_vm_action(const char *action_str,
                                    unsigned long target_vmid)
{
    unsigned long fid;
    long ret;

    if (strncmp(action_str, "stop", 4) == 0 ||
        strncmp(action_str, "down", 4) == 0 ||
        strncmp(action_str, "powerdown", 9) == 0) {
        fid = HVC_VM_POWER_DOWN;
    } else if (strncmp(action_str, "reset", 5) == 0 ||
               strncmp(action_str, "restart", 7) == 0) {
        fid = HVC_VM_RESTART;
    } else if (strncmp(action_str, "start", 5) == 0 ||
               strncmp(action_str, "up", 2) == 0 ||
               strncmp(action_str, "powerup", 7) == 0) {
        fid = HVC_VM_POWER_UP;
    } else {
        pr_err("hasee_vm_ctl: unsupported action=%s\n", action_str);
        return -EINVAL;
    }

    pr_info("hasee_vm_ctl: HVC request action=%s target_vm=%lu\n",
            action_str, target_vmid);

    ret = hvc_call3(fid, target_vmid, 0, 0);

    pr_info("hasee_vm_ctl: HVC done action=%s target_vm=%lu ret=%ld\n",
            action_str, target_vmid, ret);

    return ret;
}

static ssize_t hasee_proc_write(struct file *file,
                                const char __user *buf,
                                size_t count,
                                loff_t *ppos)
{
    char kbuf[64];
    char action[16];
    unsigned long target_vmid = 2;
    long ret;

    if (count >= sizeof(kbuf))
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    memset(action, 0, sizeof(action));

    if (sscanf(kbuf, "%15s %lu", action, &target_vmid) < 1) {
        pr_err("hasee_vm_ctl: invalid command\n");
        return -EINVAL;
    }

    if (strncmp(action, "probe", 5) == 0) {
        hasee_probe_current_vm();
        return count;
    }

    ret = hasee_request_vm_action(action, target_vmid);

    /*
     * For debug convenience, return count even if HVC ret is negative.
     * The exact HVC return code is printed in dmesg.
     */
    return count;
}

static const struct file_operations hasee_proc_fops = {
    .owner = THIS_MODULE,
    .write = hasee_proc_write,
};

static int __init hasee_vm_ctl_init(void)
{
    hasee_proc = proc_create(PROC_NAME, 0222, NULL, &hasee_proc_fops);
    if (!hasee_proc) {
        pr_err("hasee_vm_ctl: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    pr_info("hasee_vm_ctl: loaded\n");
    pr_info("hasee_vm_ctl: usage examples:\n");
    pr_info("  echo probe > /proc/%s\n", PROC_NAME);
    pr_info("  echo 'stop 2' > /proc/%s\n", PROC_NAME);
    pr_info("  echo 'reset 2' > /proc/%s\n", PROC_NAME);
    pr_info("  echo 'start 2' > /proc/%s\n", PROC_NAME);

    hasee_probe_current_vm();

    return 0;
}

static void __exit hasee_vm_ctl_exit(void)
{
    if (hasee_proc)
        proc_remove(hasee_proc);

    pr_info("hasee_vm_ctl: unloaded\n");
}

module_init(hasee_vm_ctl_init);
module_exit(hasee_vm_ctl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("H-A-SEE VM1 Guardian control module using existing Minos VM management HVC");
MODULE_AUTHOR("H-A-SEE PoC");
