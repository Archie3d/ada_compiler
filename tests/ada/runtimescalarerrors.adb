procedure RuntimeScalarErrors is
    N : Integer := 3;
    subtype Dynamic is Integer range 1 .. N;
    C : constant Dynamic := 2;
    Flag : Boolean := True;
    subtype Wrong is Integer range 1 .. Flag;
    type Static_Only is range 1 .. Dynamic'Last;
    type Constant_Only is range 1 .. C;
    Real_Last : Float := 3.0;
    subtype Real_Range is Float range 1.0 .. Real_Last;
    Anonymous : Integer range 1 .. N;
    Width : Integer := Dynamic'Width;
begin
    case N is
        when Dynamic => null;
        when others => null;
    end case;
    case N is
        when Dynamic'First => null;
        when others => null;
    end case;
    Dynamic'Output (1, 1);
end RuntimeScalarErrors;
