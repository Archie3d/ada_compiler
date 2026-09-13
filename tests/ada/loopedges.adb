-- Empty loops skip their bodies; dynamic bounds are evaluated once.
with Ada.Text_IO; use Ada.Text_IO;
procedure LoopEdges is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    type Colour is (Red, Green, Blue);
    Low : Integer := 1;
    High : Integer := 3;
    Calls : Integer := 0;
    Count : Integer := 0;
    function Bound (Value : Integer) return Integer is
    begin
        Calls := Calls + 1;
        return Value;
    end Bound;
begin
    for I in 3 .. 2 loop
        raise Program_Error;
    end loop;
    for I in reverse 3 .. 2 loop
        raise Program_Error;
    end loop;
    while False loop
        raise Program_Error;
    end loop;
    for I in Bound (Low) .. Bound (High) loop
        Count := Count + 1;
        Low := 10;
        High := 20;
    end loop;
    Check (Calls = 2 and Count = 3);
    Calls := 0;
    Count := 0;
    Low := 1;
    High := 3;
    for I in reverse Bound (Low) .. Bound (High) loop
        Count := Count + I;
        Low := 10;
        High := 20;
    end loop;
    Check (Calls = 2 and Count = 6);
    Count := 0;
    for C in Colour loop
        Count := Count + 1;
    end loop;
    Check (Count = 3);
    Put_Line ("loopedges: passed");
end LoopEdges;
