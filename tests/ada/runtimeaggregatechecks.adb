with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeAggregateChecks is
    type Vector is array (Integer range <>) of Integer;
    Size : Integer := 3;
    Values : Vector (1 .. Size) := (others => 7);
    function Fail return Integer is
    begin
        raise Constraint_Error;
        return 0;
    end Fail;
begin
    begin
        Values := (1, 2);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("positional length checked");
    end;
    begin
        Values := (0 => 1, others => 2);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("explicit choice checked");
    end;
    begin
        Values := (1, Fail, 3);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("component exception propagated");
    end;
    if Values (1) /= 7 or Values (2) /= 7 or Values (3) /= 7 then
        raise Program_Error;
    end if;
    Put_Line ("failed aggregate preserves target");
end RuntimeAggregateChecks;
