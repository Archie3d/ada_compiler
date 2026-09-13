with Ada.Text_IO;
procedure Importedfloatabi is
    function Single (X : Float) return Float;
    pragma Import (C, Single, "abiFloat");
    function Double (X : Long_Float) return Long_Float;
    pragma Import (C, Double, "abiDouble");
    function Mixed_Single (X : Float; Y : Long_Float; Z : Float) return Float;
    pragma Import (C, Mixed_Single, "abiMixedFloat");
    function Mixed_Double (X : Long_Float; Y : Float; Z : Long_Float) return Long_Float;
    pragma Import (C, Mixed_Double, "abiMixedDouble");
    procedure Update (X : Float; Y : Long_Float;
                      Result32 : out Float; Result64 : out Long_Float);
    pragma Import (C, Update, "abiUpdate");
    type Real32 is digits 6;
    type Real64 is digits 7;
    subtype Narrow32 is Real32 range 1.0 .. 2.0;
    subtype Narrow64 is Real64 range 1.0 .. 2.0;
    function Custom_Single (X : Narrow32'Base) return Real32'Base;
    pragma Import (C, Custom_Single, "abiFloat");
    function Custom_Double (X : Narrow64'Base) return Real64'Base;
    pragma Import (C, Custom_Double, "abiDouble");
    F : Float := 4.0;
    D : Long_Float := 4.0;
    X : Real32'Base := 4.0;
    Y : Real64'Base := 4.0;
begin
    F := Single (F);
    D := Double (D);
    if F /= 5.0 or D /= 5.0 or Single (8.0) /= 9.0 or Double (8.0) /= 9.0 then
        raise Program_Error;
    end if;
    if Mixed_Single (F, D, F) /= 30.0 or Mixed_Double (D, F, D) /= 30.0 then
        raise Program_Error;
    end if;
    if Custom_Single (X) /= 5.0 or Custom_Double (Y) /= 5.0 then
        raise Program_Error;
    end if;
    Update (F, D, F, D);
    if F /= 7.0 or D /= 8.0 then
        raise Program_Error;
    end if;
    if Single (Float (D)) /= 9.0 or Double (Long_Float (F)) /= 8.0 then
        raise Program_Error;
    end if;
    Ada.Text_IO.Put_Line ("imported float ABI ok");
end Importedfloatabi;
