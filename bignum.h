#include <linux/slab.h>
#include <linux/string.h>

typedef struct bignum bignum;
struct bignum {
    unsigned char *arr;
    int len;
};

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define INIT(ptr, n)                                                   \
    do {                                                               \
        (ptr) = kmalloc(sizeof(*(ptr)), GFP_KERNEL);                   \
        (ptr)->arr = kmalloc((n) * sizeof(unsigned char), GFP_KERNEL); \
        memset((ptr)->arr, 0, (n) * sizeof(unsigned char));            \
        (ptr)->len = (n);                                              \
    } while (0)
#define FREE(ptr)          \
    do {                   \
        kfree((ptr)->arr); \
        kfree((ptr));      \
    } while (0)
#define RESIZE(ptr, n)                                                     \
    do {                                                                   \
        (ptr)->arr =                                                       \
            krealloc((ptr)->arr, (n) * sizeof(unsigned char), GFP_KERNEL); \
        (ptr)->len = (n);                                                  \
    } while (0)
#define REDUCE_LENGTH(ptr)                                          \
    do {                                                            \
        while ((ptr)->len > 1 && (ptr)->arr[(ptr)->len - 1] == 0 && \
               !((ptr)->arr[(ptr)->len - 2] & (1 << 7)))            \
            (ptr)->len--;                                           \
    } while (0)

bignum *addsub(bignum *a, bignum *b);
bignum *mul(bignum *a, bignum *b);
bignum * not(bignum * a);
bignum *twoComp(bignum *a);
bignum *Lshift(bignum *a);
bignum *Rshift(bignum *a);
bignum *fib_bignum(int n);
bignum *fib_bignum_fastdouble(uint64_t target);
char *convert2Hex(bignum *a);