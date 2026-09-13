with Ada.Numerics;
with Ada.Numerics.Generic_Elementary_Functions;
package body Numerics_Checks is
    package Math is new Ada.Numerics.Generic_Elementary_Functions (Real);
    use Math;
    procedure Close (Actual, Expected : Real'Base) is
        Tolerance : Real'Base;
    begin
        if Real'Base'Size = Float'Size then
            Tolerance := 0.00001;
        else
            Tolerance := 0.000000000001;
        end if;
        if abs (Actual - Expected) > Tolerance then
            raise Program_Error;
        end if;
    end Close;
    procedure Run is
        X : Real'Base := 0.5;
        Result : Real'Base;
    begin
        Close (Sqrt (X), 0.7071067811865476);
        Close (Log (X), -0.6931471805599453);
        Close (Exp (X), 1.6487212707001282);
        Close (Sin (X), 0.4794255386042030);
        Close (Cos (X), 0.8775825618903728);
        Close (Tan (X), 0.5463024898437905);
        Close (Cot (X), 1.8304877217124520);
        Close (Arcsin (X), 0.5235987755982988);
        Close (Arccos (X), 1.0471975511965976);
        Close (Sinh (X), 0.5210953054937474);
        Close (Cosh (X), 1.1276259652063807);
        Close (Tanh (X), 0.4621171572600097);
        Close (Coth (X), 2.1639534137386529);
        Close (Arcsinh (X), 0.4812118250596035);
        Close (Arccosh (2.0), 1.3169578969248166);
        Close (Arctanh (X), 0.5493061443340549);
        Close (Arccoth (2.0), 0.5493061443340549);
        Close (Log (8.0, 2.0), 3.0);
        Close ("**" (9.0, 0.5), 3.0);
        Close (Arctan (1.0, -1.0), 2.356194490192345);
        Close (Arccot (-1.0, 1.0), 2.356194490192345);
        Close (Sin (30.0, 360.0), 0.5);
        Close (Cos (60.0, 360.0), 0.5);
        Close (Tan (45.0, 360.0), 1.0);
        Close (Cot (45.0, 360.0), 1.0);
        Close (Arcsin (X, 360.0), 30.0);
        Close (Arccos (X, 360.0), 60.0);
        Close (Arctan (1.0, -1.0, 360.0), 135.0);
        Close (Arccot (-1.0, 1.0, 360.0), 135.0);
        if Sin (180.0, 360.0) /= 0.0 or Sin (90.0, 360.0) /= 1.0
            or Cos (90.0, 360.0) /= 0.0 or Cos (180.0, 360.0) /= -1.0
            or Tan (180.0, 360.0) /= 0.0 or Cot (90.0, 360.0) /= 0.0 then
            raise Program_Error;
        end if;
        begin
            Result := Sqrt (-1.0);
            raise Program_Error;
        exception
            when Ada.Numerics.Argument_Error => null;
        end;
        begin
            Result := Log (0.0, 1.0);
            raise Program_Error;
        exception
            when Ada.Numerics.Argument_Error => null;
        end;
        begin
            Result := Sin (X, 0.0);
            raise Program_Error;
        exception
            when Ada.Numerics.Argument_Error => null;
        end;
        begin
            Result := Tan (90.0, 360.0);
            raise Program_Error;
        exception
            when Constraint_Error => null;
        end;
        begin
            Result := Cot (180.0, 360.0);
            raise Program_Error;
        exception
            when Constraint_Error => null;
        end;
        begin
            Result := Arctanh (1.0);
            raise Program_Error;
        exception
            when Constraint_Error => null;
        end;
        if Real'Base'Size = Float'Size then
            begin
                Result := Exp (100.0);
                raise Program_Error;
            exception
                when Constraint_Error => null;
            end;
        else
            Result := Exp (100.0);
            if Result < 1.0E+40 then
                raise Program_Error;
            end if;
        end if;
    end Run;
end Numerics_Checks;
