-- Nested writes reach the correct enclosing activation during recursion.
with Ada.Text_IO; use Ada.Text_IO;
procedure NestedCaptures is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    procedure Run (Seed, Depth : Integer) is
        Value : Integer := Seed;
        procedure Add is
        begin
            Value := Value + 1;
        end Add;
        procedure Deeper is
            procedure Inner is
            begin
                Add;
                Value := Value + 2;
            end Inner;
        begin
            Inner;
        end Deeper;
    begin
        Add;
        Deeper;
        if Depth > 0 then
            Run (Seed + 10, Depth - 1);
        end if;
        Check (Value = Seed + 4);
    end Run;
begin
    Run (10, 3);
    Run (100, 0);
    Put_Line ("nestedcaptures: passed");
end NestedCaptures;
