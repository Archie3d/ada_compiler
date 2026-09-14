with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeScalarChecks is
    N : Integer := 3;
    subtype Small is Integer range 2 .. N;
    function C_Abs (Value : Integer) return Small;
    pragma Import (C, C_Abs, "abs");
    Count : Integer := 0;
    X : Small := 2;
    procedure Accept_Value (V : Small := 4) is
    begin
        raise Program_Error;
    end Accept_Value;
    function Wrong return Small is
    begin
        return 4;
    end Wrong;
    function Fail return Integer is
    begin
        raise Constraint_Error;
        return 2;
    end Fail;
begin
    begin
        declare
            subtype Bad is Small range 1 .. N;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            subtype Bad is Small range 2 .. 4;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            subtype Bad is Integer range Fail .. N;
        begin
            raise Program_Error;
        exception
            when Constraint_Error => raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        X := 4;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        N := Small (4);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        N := Small'(4);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Accept_Value (4);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Accept_Value;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        N := Wrong;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            Y : Small := 4;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            type Vector is array (Small range 1 .. 2) of Integer;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            type Open_Vector is array (Small range <>) of Integer;
            subtype Bad is Open_Vector (1 .. 2);
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            type Holder is record
                Value : Small := 4;
            end record;
            Y : Holder;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        for I in Small range 1 .. 2 loop
            raise Program_Error;
        end loop;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            Wide : Long_Integer := 4_294_967_299;
        begin
            -- Its low word is 3: check the full value before narrowing.
            X := Small (Wide);
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    declare
        subtype Empty is Small range 10 .. 5;
    begin
        if Empty'First /= 10 or Empty'Last /= 5 or 7 in Empty then
            raise Program_Error;
        end if;
        begin
            N := Empty (7);
        exception
            when Constraint_Error => Count := Count + 1;
        end;
    end;
    begin
        N := C_Abs (-4);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if Count /= 17 or X /= 2 or N /= 3 then
        raise Program_Error;
    end if;
    Put_Line ("runtime scalar checks ok");
end RuntimeScalarChecks;
