#include <stdio.h>
#include <math.h>

// Kaleidoscope extern 函数实现
double print(double x) {
    printf("%f\n", x);
    return x;
}

double printnum(double n) {
    printf("%f\n", n);
    return n;
}

// Kaleidoscope 顶层匿名表达式入口
extern double __anon_expr(void);

// def testBinary(l,r) l % r;
extern double testBinary(double, double);

int main(void) {
    __anon_expr();
    return 0;
}
