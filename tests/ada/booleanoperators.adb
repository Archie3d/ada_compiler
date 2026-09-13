-- Truth tables and eager operand evaluation; no operand order is assumed.
with Ada.Text_IO; use Ada.Text_IO;
procedure BooleanOperators is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Calls : Integer := 0;
    Flag : Boolean;
    function Counted (Value : Boolean) return Boolean is
    begin
        Calls := Calls + 1;
        return Value;
    end Counted;
begin
    for Left in Boolean loop
        for Right in Boolean loop
            Check ((Left and Right) = (Left and then Right));
            Check ((Left or Right) = (Left or else Right));
            Check ((Left xor Right) = (Left /= Right));
            Check ((not Left) = (Left = False));
            Calls := 0;
            Flag := Counted (Left) and Counted (Right);
            Check (Calls = 2 and Flag = (Left and Right));
            Calls := 0;
            Flag := Counted (Left) or Counted (Right);
            Check (Calls = 2 and Flag = (Left or Right));
        end loop;
    end loop;
    Put_Line ("booleanoperators: passed");
end BooleanOperators;
