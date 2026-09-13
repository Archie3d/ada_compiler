Ada.Numerics provides Pi, e, and Argument_Error. Its Generic_Elementary_Functions
child supplies square root, logarithms, exponential, real power, trigonometric
functions (radians or an explicit Cycle), and hyperbolic functions and inverses.
Elementary_Functions instantiates it for Float; Long_Elementary_Functions for
Long_Float. Other floating point types can instantiate the generic directly.

Example:

```ada
with Ada.Numerics.Elementary_Functions;
procedure Example is
    use Ada.Numerics.Elementary_Functions;
    Root : Float := Sqrt (2.0);
    Sine : Float := Sin (30.0, Cycle => 360.0);
    Cube : Float := 2.0 ** 3.0;
    Root_Again : Float := "**" (2.0, 0.5);
begin
    null;
end Example;
```

Compatibility with this compiler:

- Real exponentiation uses the standard "**" operator. Make the package
  directly visible with a use clause for infix calls, or use a qualified call
  such as Ada.Numerics.Elementary_Functions."**" (2.0, 0.5).
- Profiles use Float_Type'Base, so a constrained actual subtype does not
  constrain arguments, results, or intermediate calculations. Intermediate
  calculations use C double.
- Invalid domains raise Ada.Numerics.Argument_Error. Poles and non-finite C
  results raise Constraint_Error; underflow to zero is permitted.
- Calculations use the platform libm at double precision; this is not a claim
  of conformance to the accuracy requirements of the optional Numerics Annex.
- On Linux and other systems with a separate libm, link generated assembly
  with `cc program.s /path/to/libadart.a -lm -o program`. The existing Ada
  driver's link command does not pass -lm. On macOS its normal command works.

The C mappings are private implementation support: Ada wrappers validate domains
before calling them. They use the existing pending-exception runtime mechanism.
