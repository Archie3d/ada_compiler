with Ada.Text_IO; use Ada.Text_IO;
procedure InferredMatrixChecks is
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    Bound_Calls : Integer := 0;
    Value_Calls : Integer := 0;
    function Bound (Value : Integer) return Integer is
    begin
        Bound_Calls := Bound_Calls + 1;
        return Value;
    end Bound;
    function Cell return Integer is
    begin
        if Bound_Calls /= 4 then
            raise Program_Error;
        end if;
        Value_Calls := Value_Calls + 1;
        return Value_Calls;
    end Cell;
    A : Matrix := (Bound (2) .. Bound (3) => (Bound (5) .. Bound (7) => Cell));
    Target : Matrix (1 .. 2, 1 .. 2) := (others => (others => 9));
begin
    if Bound_Calls /= 4 or Value_Calls /= 6 or A (3, 7) /= 6 then
        raise Program_Error;
    end if;
    Put_Line ("choices run once before component values");
    Bound_Calls := 0;
    Value_Calls := 0;
    declare
        Empty : Matrix := (Bound (2) .. Bound (1) => (Bound (5) .. Bound (7) => Cell));
    begin
        if Empty'Length (1) /= 0 or Empty'Length (2) /= 3
            or Bound_Calls /= 4 or Value_Calls /= 0 then
            raise Program_Error;
        end if;
    end;
    Bound_Calls := 0;
    declare
        Empty : Matrix := (Bound (2) .. Bound (3) => (Bound (5) .. Bound (4) => Cell));
    begin
        if Empty'Length (1) /= 2 or Empty'Length (2) /= 0
            or Bound_Calls /= 4 or Value_Calls /= 0 then
            raise Program_Error;
        end if;
    end;
    Put_Line ("null dimensions preserve bounds and skip component values");
    Bound_Calls := 0;
    begin
        declare
            Bad : Matrix := (1 => (Bound (5) .. Bound (6) => Cell),
                             2 => (Bound (6) .. Bound (7) => Cell));
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Put_Line ("equal-length rows require identical bounds");
    end;
    if Value_Calls /= 0 or Bound_Calls /= 4 then
        raise Program_Error;
    end if;
    begin
        declare
            Bad : Matrix := ((Cell, Cell), (Cell, Cell, Cell));
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Put_Line ("ragged rows rejected before component values");
    end;
    begin
        declare
            type Cube is array (Positive range <>, Positive range <>, Positive range <>) of Integer;
            Bad : Cube := (1 => (1 => (1 .. 2 => Cell)), 2 => (1 => (2 .. 3 => Cell)));
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Put_Line ("deep subaggregate bounds checked");
    end;
    begin
        Target := (1 => (1 .. 2 => Cell), 2 => (2 .. 3 => Cell));
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("contextual row bounds checked before assignment");
    end;
    if Value_Calls /= 0 or Target (1, 1) /= 9 or Target (2, 2) /= 9 then
        raise Program_Error;
    end if;
    begin
        declare
            Bad : Matrix := (1 => (5 .. 4 => Cell), 2 => (6 .. 5 => Cell));
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Put_Line ("null row bounds must also match");
    end;
    begin
        declare
            subtype Small is Integer range 1 .. 2;
            type Small_Matrix is array (Small range <>, Small range <>) of Integer;
            Bad : Small_Matrix := ((Cell, Cell, Cell), (Cell, Cell, Cell));
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Put_Line ("inferred bounds respect the index subtype");
    end;
    if Value_Calls /= 0 then
        raise Program_Error;
    end if;
end InferredMatrixChecks;
