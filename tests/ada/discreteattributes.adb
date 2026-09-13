-- Discrete attributes round-trip values and check type boundaries.
with Ada.Text_IO; use Ada.Text_IO;
procedure DiscreteAttributes is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    type Colour is (Red, Green, Blue);
    C : Colour := Blue;
    Bad : Integer := 3;
    Letter : Character := Character'Last;
begin
    for C in Colour loop
        Check (Colour'Val (Colour'Pos (C)) = C);
    end loop;
    Check (Colour'Succ (Red) = Green and Colour'Pred (Blue) = Green);
    Check (Character'Val (Character'Pos ('Z')) = 'Z');
    Check (Character'Succ ('a') = 'b' and Character'Pred ('b') = 'a');
    begin
        C := Colour'Val (Bad);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        C := Colour'Succ (C);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    C := Red;
    begin
        C := Colour'Pred (C);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        Letter := Character'Succ (Letter);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Letter := Character'First;
    begin
        Letter := Character'Pred (Letter);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Put_Line ("discreteattributes: passed");
end DiscreteAttributes;
