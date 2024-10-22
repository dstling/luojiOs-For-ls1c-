
#ifndef _MATH_
#define _MATH_

/*
 * 如果需要使用下面函数，但又没有该函数的实现。
 * 那么可以从pmon的源码目录下的lib目录下的libm目录中拷贝相应的源码
 * pmon中的libm下的源文件几乎都是以函数名命名的
 */

#ifdef FLOAT
float sin(float x);
float cos(float x);
float tan(float x);
float atan(float x);
float exp(float x);
float log(float x);
float pow(float x);
float sqrt(float x);
#else


double acos(double x);
double acosh(double x);
double asin(double x);
double atan(double x);
double atan2(double y, double x);
double atanh(double x);
double ceil(double x);
double cos(double x);
double cosh(double x);
double exp(double x);
double fabs(double x);
double floor(double x);
double fmod(double x, double y);
double frexp(double x, int *i);
double ldexp(double x, int i);
double log(double x);
double log10(double x);
double modf(double x, double *i);
double pow(double x,double y);
double sin(double x);
double sinh(double x);
double sqrt(double x);
double tan(double x);
double tanh(double x);

#endif


float acosf(float x);
float asinf(float x);
float atan2f(float y, float x);
float atanf(float x);
float ceilf(float x);
float cosf(float x);
float coshf(float x);
float expf(float x);
float fabsf(float x);
float floorf(float x);
float fmodf(float x, float y);
float frexpf(float x, int *i);
float ldexpf(float x, int i);
float log10f(float x);
float logf(float x);
float modff(float x, float *i);
float powf(float x);
float sinf(float x);
float sinhf(float x);
float sqrtf(float x);
float tanf(float x);
float tanhf(float x);




double scalbn (double x, int n);
double copysign(double x, double y);



#endif

