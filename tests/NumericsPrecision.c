#include "adanumerics.h"
#include "adart.h"

#include <assert.h>
#include <float.h>
#include <math.h>

static int pendingException;

void __ada_raise(int id)
{
    pendingException = id;
}

static void expectFloatPole(float (*function)(float), float argument)
{
    pendingException = 0;
    function(argument);
    assert(pendingException == ADA_CONSTRAINT_ERROR);
}

static void expectDoublePole(double (*function)(double), double argument)
{
    pendingException = 0;
    function(argument);
    assert(pendingException == ADA_CONSTRAINT_ERROR);
}

int main(void)
{
    assert(signbit(__ada_numerics_sqrt_f32(-0.0f)));
    assert(signbit(__ada_numerics_sin_f32(-0.0f)));
    assert(signbit(__ada_numerics_arctan_f32(-0.0f, 1.0f)));
    assert(signbit(__ada_numerics_sin_cycle_f32(-0.0f, 360.0f)));
    assert(signbit(__ada_numerics_sin(-0.0)));
    assert(__ada_numerics_sqrt_f32(FLT_MAX) > 1.0f);
    assert(isfinite(__ada_numerics_sin_cycle_f32(FLT_MAX, FLT_MIN)));
    assert(pendingException == 0);

    for (int i = -12; i <= 12; ++i) {
        float angle = (float)i * 90.0f;
        float sine = __ada_numerics_sin_cycle_f32(angle, 360.0f);
        float cosine = __ada_numerics_cos_cycle_f32(angle, 360.0f);
        assert(sine == (i % 2 == 0 ? 0.0f : (i % 4 == 1 || i % 4 == -3 ? 1.0f : -1.0f)));
        assert(cosine == (i % 2 != 0 ? 0.0f : (i % 4 == 0 ? 1.0f : -1.0f)));
        if (i % 2 == 0) {
            assert(__ada_numerics_tan_cycle_f32(angle, 360.0f) == 0.0f);
        } else {
            assert(__ada_numerics_cot_cycle_f32(angle, 360.0f) == 0.0f);
        }
    }
    assert(pendingException == 0);
    expectFloatPole(__ada_numerics_log_f32, 0.0f);
    expectFloatPole(__ada_numerics_cot_f32, 0.0f);
    expectFloatPole(__ada_numerics_coth_f32, 0.0f);
    expectFloatPole(__ada_numerics_arctanh_f32, 1.0f);
    expectFloatPole(__ada_numerics_arccoth_f32, -1.0f);
    expectFloatPole(__ada_numerics_exp_f32, 100.0f);
    expectFloatPole(__ada_numerics_cosh_f32, 100.0f);
    expectFloatPole(__ada_numerics_sinh_f32, 100.0f);
    pendingException = 0;
    assert(__ada_numerics_exp(100.0) > 1.0e40);
    assert(pendingException == 0);
    assert(__ada_numerics_power_f32(FLT_MAX, 2.0f) == 0.0f);
    assert(pendingException == ADA_CONSTRAINT_ERROR);
    pendingException = 0;
    assert(__ada_numerics_power((double)FLT_MAX, 2.0) > (double)FLT_MAX);
    assert(pendingException == 0);
    assert(__ada_numerics_exp_f32(-1000.0f) == 0.0f);
    assert(pendingException == 0);
    __ada_numerics_tan_cycle_f32(90.0f, 360.0f);
    assert(pendingException == ADA_CONSTRAINT_ERROR);
    expectDoublePole(__ada_numerics_log, 0.0);
    expectDoublePole(__ada_numerics_exp, 1000.0);
    pendingException = 0;
    assert(__ada_numerics_exp(-1000.0) == 0.0);
    assert(pendingException == 0);
    return 0;
}
