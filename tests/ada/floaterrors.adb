-- Every line below breaks one of the rules Ada places on real types.
procedure Floaterrors is

    type Coefficient is digits 10 range -1.0 .. 1.0;
    type Too_Wide is digits 20;
    type Whole_Bounds is digits 6 range 0 .. 1;

    C : Coefficient := 1;
    N : Integer := 2;

begin
    C := C mod 0.5;
    C := C * N;
    C := C ** 0.5;
    N := N + 1.0;
    N := Integer'Digits;
end Floaterrors;
