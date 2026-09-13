/* Double-precision libm mappings used by Ada.Numerics. Domain checks
   belong to the Ada wrappers so Argument_Error retains its Ada identity.
   Poles and non-finite results raise the existing Constraint_Error. */
#ifndef ADANUMERICS_H
#define ADANUMERICS_H

double __ada_numerics_sqrt(double x);
double __ada_numerics_log(double x);
double __ada_numerics_exp(double x);
double __ada_numerics_sin(double x);
double __ada_numerics_cos(double x);
double __ada_numerics_tan(double x);
double __ada_numerics_cot(double x);
double __ada_numerics_arcsin(double x);
double __ada_numerics_arccos(double x);
double __ada_numerics_sinh(double x);
double __ada_numerics_cosh(double x);
double __ada_numerics_tanh(double x);
double __ada_numerics_coth(double x);
double __ada_numerics_arcsinh(double x);
double __ada_numerics_arccosh(double x);
double __ada_numerics_arctanh(double x);
double __ada_numerics_arccoth(double x);
double __ada_numerics_log_base(double x, double y);
double __ada_numerics_power(double x, double y);
double __ada_numerics_sin_cycle(double x, double y);
double __ada_numerics_cos_cycle(double x, double y);
double __ada_numerics_tan_cycle(double x, double y);
double __ada_numerics_cot_cycle(double x, double y);
double __ada_numerics_arcsin_cycle(double x, double y);
double __ada_numerics_arccos_cycle(double x, double y);
double __ada_numerics_arctan(double x, double y);
double __ada_numerics_arctan_cycle(double x, double y, double cycle);

#endif
