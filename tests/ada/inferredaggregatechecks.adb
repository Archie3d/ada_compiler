with Ada.Text_IO; use Ada.Text_IO;
procedure InferredAggregateChecks is
    type Vector is array (Positive range <>) of Integer;
    Calls : Integer := 0;
    function Fail return Integer is
    begin
        Calls := Calls + 1;
        raise Constraint_Error;
        return 0;
    end Fail;
    function Make (Low, High : Integer) return Vector is
    begin
        return (Low .. High => Fail);
    end Make;
    procedure Use_Value (Value : Vector) is
    begin
        raise Program_Error;
    end Use_Value;
    Empty : String := (0 .. -1 => 'x');
begin
    if Empty'First /= 0 or Empty'Last /= -1 or Empty'Length /= 0 then
        raise Program_Error;
    end if;
    begin
        Use_Value (Make (0, 2));
    exception
        when Constraint_Error => Put_Line ("inferred index constraint checked");
    end;
    if Calls /= 0 then
        raise Program_Error;
    end if;
    for I in 1 .. 10 loop
        begin
            Use_Value (Make (1, 3));
        exception
            when Constraint_Error => null;
        end;
    end loop;
    if Calls /= 10 then
        raise Program_Error;
    end if;
    Put_Line ("inferred aggregate component failure propagated");
end InferredAggregateChecks;
