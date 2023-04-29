#include "bignum.h"

bignum *NOT(bignum *a)
{
    bignum *b;
    INIT(b, a->len);
    for (int i = 0; i < a->len; i++)
        b->arr[i] = ~(a->arr[i]);
    return b;
}
bignum *twoComp(bignum *a)
{
    bignum *One;
    INIT(One, 1);
    One->arr[0] = 0x01;
    bignum *notA = NOT(a);
    bignum *res = addsub(notA, One);
    FREE(One);
    return res;
}
bignum *mul(bignum *a, bignum *b)
{
    bignum *c;
    INIT(c, a->len + b->len + 1);
    for (int i = 0; i < a->len; i++) {
        for (int j = 0; j < b->len; j++) {
            unsigned int A = a->arr[i];
            unsigned int B = b->arr[j];
            unsigned int C = A * B;
            c->arr[i + j] += C & 0xff;
            c->arr[i + j + 1] += (C & 0xff00) >> 8;
        }
    }
    if (c->arr[c->len - 1] & (1U << 7)) {
        RESIZE(c, c->len + 1);
        c->arr[c->len - 1] = 0;
    }
    REDUCE_LENGTH(c);
    return c;
}
bignum *addsub(bignum *a, bignum *b)
{
    unsigned char signedA = (a->arr[a->len - 1] & (1U << 7)) >> 7;
    unsigned char signedB = (b->arr[b->len - 1] & (1U << 7)) >> 7;
    unsigned int newLength = MAX(a->len, b->len);
    bignum *c;
    INIT(c, newLength);
    unsigned int carry = 0;
    for (int i = 0; i < newLength; i++) {
        unsigned int A = i < a->len ? a->arr[i] : 0xff * signedA;
        unsigned int B = i < b->len ? b->arr[i] : 0xff * signedB;
        unsigned int C = A + B + carry;
        carry = (C & (1U << 8)) >> 8;
        c->arr[i] = C & 0xff;
    }
    if (carry && !(signedA ^ signedB) &&
        (signedA ^ ((c->arr[c->len - 1] & (1U << 7)) >> 7))) {
        RESIZE(c, c->len + 1);
        c->arr[c->len - 1] = signedA ? 0x01 : 0xff;
    }
    REDUCE_LENGTH(c);
    return c;
}

bignum *Lshift(bignum *a)
{
    bignum *b;
    INIT(b, (a->len + 1));
    unsigned char carry = 0;
    for (int i = 0; i < a->len; i++) {
        unsigned int C = a->arr[i];
        b->arr[i] = ((C << 1) | carry) & 0xff;
        carry = C >> 7;
    }
    b->arr[a->len] = carry;
    REDUCE_LENGTH(b);
    return b;
}

// bignum* Rshift(bignum* a){
//     bignum* b;
//     INIT(b, a->len);
//     unsigned char carry = 0;
//     for(int i = a->len-1; i >= 0; i--){
//         unsigned int C = a->arr[i];
//         b->arr[i] = ((C >> 1) | (carry << 7)) & 0xff;
//         carry = C & 1;
//     }
//     return b;
// }

// bignum* fib_bignum(int n){
//     bignum* a;
//     bignum* b;
//     bignum* c;
//     INIT(a, 1);
//     INIT(b, 1);
//     a->arr[0] = 0x01;
//     b->arr[0] = 0x01;
//     if(!n){
//         FREE(a);
//         b->arr[0] = 0x00;
//         return b;
//     }
//     if(n <= 2){
//         FREE(a);
//         return b;
//     }
//     for(int i = 2; i < n; i++){
//         c = addsub(a, b);
//         FREE(a);
//         a = b;
//         b = c;
//     }
//     FREE(a);
//     return b;
// }

bignum *fib_bignum_fastdouble(uint64_t target)
{
    bignum *fib_n0;
    bignum *fib_n1;
    bignum *fib_2n0;
    bignum *fib_2n1;
    INIT(fib_n0, 1);
    INIT(fib_n1, 1);
    fib_n0->arr[0] = 0x01;
    fib_n1->arr[0] = 0x01;
    if (!target) {
        FREE(fib_n0);
        fib_n1->arr[0] = 0x00;
        return fib_n1;
    }
    if (target <= 2) {
        FREE(fib_n0);
        return fib_n1;
    }

    // find first 1
    uint8_t count = 63 - __builtin_clzll(target);
    // uint64_t fib_n0 = 1, fib_n1 = 1;

    for (uint64_t i = count; i-- > 0;) {
        // fib_2n0 = fib_n0 * ((fib_n1 << 1) - fib_n0);
        fib_2n0 = mul(fib_n0, addsub(Lshift(fib_n1), twoComp(fib_n0)));

        // fib_2n1 = fib_n0 * fib_n0 + fib_n1 * fib_n1;
        fib_2n1 = addsub(mul(fib_n0, fib_n0), mul(fib_n1, fib_n1));

        if (target & (1UL << i)) {
            fib_n0 = fib_2n1;
            // fib_n1 = fib_2n0 + fib_2n1;
            fib_n1 = addsub(fib_2n0, fib_2n1);
        } else {
            fib_n0 = fib_2n0;
            fib_n1 = fib_2n1;
        }
    }
    return fib_n0;
}

char *convert2Hex(bignum *a)
{
    char *str = kmalloc(a->len * 2 + 1, GFP_KERNEL);
    for (int i = 0; i < a->len; i++) {
        char lowByte = a->arr[i] & 0xff;
        char highByte = a->arr[i] >> 4;
        str[a->len * 2 - 1 - 2 * i] =
            (lowByte < 10 ? lowByte + '0' : lowByte - 10 + 'A');
        str[a->len * 2 - 2 - 2 * i] =
            (highByte < 10 ? highByte + '0' : highByte - 10 + 'A');
    }
    str[a->len * 2] = '\0';
    return str;
}