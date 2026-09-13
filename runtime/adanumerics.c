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

