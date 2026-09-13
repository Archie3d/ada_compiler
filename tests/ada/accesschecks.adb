-- Null reads and writes raise Constraint_Error; deallocation clears its actual.
with Ada.Unchecked_Deallocation;
with Ada.Text_IO; use Ada.Text_IO;
procedure AccessChecks is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    type Counter is access Integer;
    type Line is access String (1 .. 3);
    procedure Dispose is new Ada.Unchecked_Deallocation (Integer, Counter);
    P : Counter := new Integer'(7);
    Text : Line := null;
    Value : Integer;
    Letter : Character;
begin
    Dispose (P);
    Check (P = null);
    Dispose (P);
    Check (P = null);
    begin
        Value := P.all;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        P.all := 1;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        Letter := Text (1);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        Text (1) := 'x';
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Put_Line ("accesschecks: passed");
end AccessChecks;
