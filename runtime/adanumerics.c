#include "adanumerics.h"
#include "adart.h"

#include <math.h>

static const double fullCycle = 6.283185307179586476925286766559;

static double checkedResult(double value)
{
    if (!isfinite(value)) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return 0.0;
    }
    return value;
}

/* Reduce before scaling to avoid overflow for large X and small cycles.
   Handle axes explicitly: libm's approximation to pi is not an exact axis. */
static double cycleSin(double x, double cycle)
{
    double phase = remainder(x, cycle) / cycle;
    if (phase == 0.0 || fabs(phase) == 0.5) {
        return copysign(0.0, x);
    }
    if (phase == 0.25) {
        return 1.0;
    }
    if (phase == -0.25) {
        return -1.0;
    }
    return sin(phase * fullCycle);
}

static double cycleCos(double x, double cycle)
{
    double phase = remainder(x, cycle) / cycle;
    if (phase == 0.0) {
        return 1.0;
    }
    if (fabs(phase) == 0.5) {
        return -1.0;
    }
    if (fabs(phase) == 0.25) {
        return 0.0;
    }
    return cos(phase * fullCycle);
}

double __ada_numerics_sqrt(double x)
{
    return checkedResult(sqrt(x));
}

double __ada_numerics_log(double x)
{
    return checkedResult(log(x));
}

double __ada_numerics_exp(double x)
{
    return checkedResult(exp(x));
}

double __ada_numerics_sin(double x)
{
    return checkedResult(sin(x));
}

double __ada_numerics_cos(double x)
{
    return checkedResult(cos(x));
}

double __ada_numerics_tan(double x)
{
    return checkedResult(tan(x));
}

double __ada_numerics_cot(double x)
{
    return checkedResult(1.0 / tan(x));
}

double __ada_numerics_arcsin(double x)
{
    return checkedResult(asin(x));
}

double __ada_numerics_arccos(double x)
{
    return checkedResult(acos(x));
}

double __ada_numerics_sinh(double x)
{
    return checkedResult(sinh(x));
}

double __ada_numerics_cosh(double x)
{
    return checkedResult(cosh(x));
}

double __ada_numerics_tanh(double x)
{
    return checkedResult(tanh(x));
}

double __ada_numerics_coth(double x)
{
    return checkedResult(1.0 / tanh(x));
}

double __ada_numerics_arcsinh(double x)
{
    return checkedResult(asinh(x));
}

double __ada_numerics_arccosh(double x)
{
    return checkedResult(acosh(x));
}

double __ada_numerics_arctanh(double x)
{
    return checkedResult(atanh(x));
}

double __ada_numerics_arccoth(double x)
{
    return checkedResult(atanh(1.0 / x));
}

double __ada_numerics_log_base(double x, double y)
{
    return checkedResult(log(x) / log(y));
}

double __ada_numerics_power(double x, double y)
{
    return checkedResult(pow(x, y));
}

double __ada_numerics_sin_cycle(double x, double y)
{
    return checkedResult(cycleSin(x, y));
}

double __ada_numerics_cos_cycle(double x, double y)
{
    return checkedResult(cycleCos(x, y));
}

double __ada_numerics_tan_cycle(double x, double y)
{
    return checkedResult(cycleSin(x, y) / cycleCos(x, y));
}

double __ada_numerics_cot_cycle(double x, double y)
{
    return checkedResult(cycleCos(x, y) / cycleSin(x, y));
}

double __ada_numerics_arcsin_cycle(double x, double y)
{
    return checkedResult((asin(x) / fullCycle) * y);
}

double __ada_numerics_arccos_cycle(double x, double y)
{
    return checkedResult((acos(x) / fullCycle) * y);
}

double __ada_numerics_arctan(double x, double y)
{
    return checkedResult(atan2(x, y));
}

double __ada_numerics_arctan_cycle(double x, double y, double cycle)
{
    return checkedResult((atan2(x, y) / fullCycle) * cycle);
}

/* Single-precision mappings: all arithmetic and libm calls stay in float. */
static const float fullCycle32 = 6.283185307179586476925286766559f;

static float checkedResult32(float value)
{
    if (!isfinite(value)) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return 0.0f;
    }
    return value;
}

/* Reduce before scaling to avoid overflow for large X and small cycles.
   Handle axes explicitly: libm's approximation to pi is not an exact axis. */
static float cycleSin32(float x, float cycle)
{
    float phase = remainderf(x, cycle) / cycle;
    if (phase == 0.0f || fabsf(phase) == 0.5f) {
        return copysignf(0.0f, x);
    }
    if (phase == 0.25f) {
        return 1.0f;
    }
    if (phase == -0.25f) {
        return -1.0f;
    }
    return sinf(phase * fullCycle32);
}

static float cycleCos32(float x, float cycle)
{
    float phase = remainderf(x, cycle) / cycle;
    if (phase == 0.0f) {
        return 1.0f;
    }
    if (fabsf(phase) == 0.5f) {
        return -1.0f;
    }
    if (fabsf(phase) == 0.25f) {
        return 0.0f;
    }
    return cosf(phase * fullCycle32);
}

float __ada_numerics_sqrt_f32(float x)
{
    return checkedResult32(sqrtf(x));
}

float __ada_numerics_log_f32(float x)
{
    return checkedResult32(logf(x));
}

float __ada_numerics_exp_f32(float x)
{
    return checkedResult32(expf(x));
}

float __ada_numerics_sin_f32(float x)
{
    return checkedResult32(sinf(x));
}

float __ada_numerics_cos_f32(float x)
{
    return checkedResult32(cosf(x));
}

float __ada_numerics_tan_f32(float x)
{
    return checkedResult32(tanf(x));
}

float __ada_numerics_cot_f32(float x)
{
    return checkedResult32(1.0f / tanf(x));
}

float __ada_numerics_arcsin_f32(float x)
{
    return checkedResult32(asinf(x));
}

float __ada_numerics_arccos_f32(float x)
{
    return checkedResult32(acosf(x));
}

float __ada_numerics_sinh_f32(float x)
{
    return checkedResult32(sinhf(x));
}

float __ada_numerics_cosh_f32(float x)
{
    return checkedResult32(coshf(x));
}

float __ada_numerics_tanh_f32(float x)
{
    return checkedResult32(tanhf(x));
}

float __ada_numerics_coth_f32(float x)
{
    return checkedResult32(1.0f / tanhf(x));
}

float __ada_numerics_arcsinh_f32(float x)
{
    return checkedResult32(asinhf(x));
}

float __ada_numerics_arccosh_f32(float x)
{
    return checkedResult32(acoshf(x));
}

float __ada_numerics_arctanh_f32(float x)
{
    return checkedResult32(atanhf(x));
}

float __ada_numerics_arccoth_f32(float x)
{
    return checkedResult32(atanhf(1.0f / x));
}

float __ada_numerics_log_base_f32(float x, float y)
{
    return checkedResult32(logf(x) / logf(y));
}

float __ada_numerics_power_f32(float x, float y)
{
    return checkedResult32(powf(x, y));
}

float __ada_numerics_sin_cycle_f32(float x, float y)
{
    return checkedResult32(cycleSin32(x, y));
}

float __ada_numerics_cos_cycle_f32(float x, float y)
{
    return checkedResult32(cycleCos32(x, y));
}

float __ada_numerics_tan_cycle_f32(float x, float y)
{
    return checkedResult32(cycleSin32(x, y) / cycleCos32(x, y));
}

float __ada_numerics_cot_cycle_f32(float x, float y)
{
    return checkedResult32(cycleCos32(x, y) / cycleSin32(x, y));
}

float __ada_numerics_arcsin_cycle_f32(float x, float y)
{
    return checkedResult32((asinf(x) / fullCycle32) * y);
}

float __ada_numerics_arccos_cycle_f32(float x, float y)
{
    return checkedResult32((acosf(x) / fullCycle32) * y);
}

float __ada_numerics_arctan_f32(float x, float y)
{
    return checkedResult32(atan2f(x, y));
}

float __ada_numerics_arctan_cycle_f32(float x, float y, float cycle)
{
    return checkedResult32((atan2f(x, y) / fullCycle32) * cycle);
}

