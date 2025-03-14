#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "bignum.h"
#include "bn_kernel.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500
#define TIME_MEASSURE

enum functionOptions { FIB_SEQUENCE, FIB, BN_KERNEL, FAST_DOUBLELING_ITER };

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static void reverse_str(char *str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}
static void string_number_add(char *a, char *b, char *c)
{
    if (strlen(a) < strlen(b)) {
        char *tmp = a;
        a = b;
        b = tmp;
    }
    size_t lenA = strlen(a);
    size_t lenB = strlen(b);

    reverse_str(a);
    reverse_str(b);

    int carry = 0, i = 0;
    for (i = 0; i < lenA; i++) {
        int numA = a[i] - '0';
        int numB = (i < lenB ? b[i] - '0' : 0);
        c[i] = numA + numB + carry;
        carry = c[i] / 10;
        c[i] %= 10;
        c[i] += '0';
    }

    if (carry)
        c[i++] = '1';
    c[i] = '\0';
    reverse_str(a);
    reverse_str(b);
    reverse_str(c);
}
static long long fib(long long k, void *buf)
{
    char *a = kmalloc(128 * sizeof(char), GFP_KERNEL);
    char *b = kmalloc(128 * sizeof(char), GFP_KERNEL);
    char *c = kmalloc(128 * sizeof(char), GFP_KERNEL);
    a[0] = '0';
    b[0] = '1';
    a[1] = b[1] = '\0';
    if (k == 0) {
        copy_to_user(buf, a, sizeof(char) * 128);
        return strlen(a);
    }
    if (k == 1) {
        copy_to_user(buf, b, sizeof(char) * 128);
        return strlen(b);
    }

    for (int i = 2; i <= k; i++) {
        string_number_add(a, b, c);
        char *tmp = a;
        a = b;
        b = c;
        c = tmp;
    }

    if (__copy_to_user(buf, b, strlen(b) + 1)) {
        kfree(a);
        kfree(b);
        kfree(c);
        return -EFAULT;
    }
    size_t len = strlen(b);
    kfree(a);
    kfree(b);
    kfree(c);
    return len;
}

static inline uint64_t fast_doubling_iter(uint64_t target)
{
    if (target <= 2)
        return !!target;

    // find first 1
    uint8_t count = 63 - __builtin_clzll(target);
    uint64_t fib_n0 = 1, fib_n1 = 1;

    for (uint64_t i = count, fib_2n0, fib_2n1, mask; i-- > 0;) {
        fib_2n0 = fib_n0 * ((fib_n1 << 1) - fib_n0);
        fib_2n1 = fib_n0 * fib_n0 + fib_n1 * fib_n1;

        mask = -!!(target & (1UL << i));
        fib_n0 = (fib_2n0 & ~mask) + (fib_2n1 & mask);
        fib_n1 = (fib_2n0 & mask) + fib_2n1;
    }
    return fib_n0;
}

static long long bn_fib_loader(long long int k, void *buf)
{
    bn *fibptr = bn_alloc(1);
    bn_fib_fdoubling(fibptr, k);
    char *p = bn_to_string(fibptr);
    size_t len = strlen(p) + 1;
    size_t left = copy_to_user(buf, p, len);
    bn_free(fibptr);
    kfree(p);
    return left;  // return number of bytes that could not be copied
}
static long long bignum_fib_loader(long long int k, void *buf)
{
    bignum *res = fib_bignum_fastdouble(k);
    char *p = convert2Hex(res);
    size_t len = strlen(p) + 1;
    size_t left = copy_to_user(buf, p, len);
    FREE(res);
    kfree(p);
    return left;
}

#ifdef TIME_MEASSURE
static ktime_t kt;

static long long fib_time_proxy(long long k,
                                char *buf,
                                int func)  // func: choose which function to use
{
    kt = ktime_get();
    long long result;
    switch (func) {
    case 0:
        result = fib_sequence(k);
        break;
    case 1:
        result = fib(k, buf);
        break;
    case 2:
        result = fast_doubling_iter(k);
        break;
    case 3:
        result = bn_fib_loader(k, buf);
        break;

    case 4:
        result = bignum_fib_loader(k, buf);
        break;
    }
    kt = ktime_sub(ktime_get(), kt);

    return result;
}

static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_time_proxy(*offset, buf, size);
}

static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}
#endif

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

#ifndef TIME_MEASSURE
/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // return (ssize_t) fib_sequence(*offset);
    // return (ssize_t) fast_doubling_iter(*offset);
    return (ssize_t) fib(*offset, buf);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}
#endif

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
