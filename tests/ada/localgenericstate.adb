with Ada.Text_IO; use Ada.Text_IO;
procedure LocalGenericState is
    Elaborations : Integer := 0;
    procedure Run (Seed, Depth : Integer) is
        generic
        package Template is
            Value : Integer := Seed;
            procedure Add;
        end Template;
        package body Template is
            procedure Add is
            begin
                Value := Value + 1;
            end Add;
        begin
            Elaborations := Elaborations + 1;
        end Template;
        package First is new Template;
        package Second is new Template;
    begin
        if First.Value /= Seed or Second.Value /= Seed then
            raise Program_Error;
        end if;
        First.Add;
        if Depth > 0 then
            Run (Seed + 10, Depth - 1);
        end if;
        if First.Value /= Seed + 1 or Second.Value /= Seed then
            raise Program_Error;
        end if;
    end Run;
begin
    if Elaborations /= 0 then
        raise Program_Error;
    end if;
    Run (5, 2);
    Run (50, 0);
    if Elaborations /= 8 then
        raise Program_Error;
    end if;
    Put_Line ("fresh generic instances, captures, and recursion");
end LocalGenericState;
