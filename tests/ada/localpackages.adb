with Ada.Text_IO; use Ada.Text_IO;
procedure LocalPackages is
    Elaborations : Integer := 0;
    procedure Run (Seed : Integer) is
        package Local is
            Value : Integer := Seed;
            procedure Add;
        private
            Step : Integer := 2;
        end Local;
        package body Local is
            procedure Add is
            begin
                Value := Value + Step;
            end Add;
        begin
            Elaborations := Elaborations + 1;
            Value := Value + 1;
        end Local;
    begin
        if Local.Value /= Seed + 1 then
            raise Program_Error;
        end if;
        Local.Add;
        if Local.Value /= Seed + 3 then
            raise Program_Error;
        end if;
        Put_Line ("fresh package and captured state");
    end Run;
begin
    Run (10);
    Run (20);
    for I in 1 .. 3 loop
        declare
            package In_Block is
                Value : Integer := I;
            end In_Block;
            package body In_Block is
            begin
                Elaborations := Elaborations + 1;
                raise Constraint_Error;
            exception
                when Constraint_Error => Value := Value + 10;
            end In_Block;
        begin
            if In_Block.Value /= I + 10 then
                raise Program_Error;
            end if;
        end;
    end loop;
    if Elaborations /= 5 then
        raise Program_Error;
    end if;
    Put_Line ("block package re-elaborated and recovered");
end LocalPackages;
