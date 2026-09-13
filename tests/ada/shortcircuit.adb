-- Short-circuit operators skip or evaluate their right operand exactly once.
with Ada.Text_IO; use Ada.Text_IO;
procedure ShortCircuit is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Calls : Integer := 0;
    Flag : Boolean;
    function Counted return Boolean is
    begin
        Calls := Calls + 1;
        return Calls = 1;
    end Counted;
    function Fails return Boolean is
    begin
        raise Constraint_Error;
        return False;
    end Fails;
begin
    Check (not (False and then Counted));
    Check (True or else Counted);
    Check (Calls = 0);
    Check (True and then Counted);
    Check (not (False or else Counted));
    Check (Calls = 2);
    Check (not (False and then Fails));
    Check (True or else Fails);
    begin
        Flag := True and then Fails;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        Flag := False or else Fails;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Put_Line ("shortcircuit: passed");
end ShortCircuit;
