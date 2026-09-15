-- A single handler choice list catches any of the exceptions it names.
with Ada.Text_IO; use Ada.Text_IO;
procedure HandlerChoices is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Alpha, Beta, Gamma : exception;
    Caught : Integer := 0;
    procedure Raise_One (Which : Integer) is
    begin
        case Which is
            when 1 => raise Alpha;
            when 2 => raise Beta;
            when others => raise Gamma;
        end case;
    end Raise_One;
    procedure Guarded (Which : Integer) is
    begin
        Raise_One (Which);
    exception
        when Alpha | Beta => Caught := Caught + 1;
    end Guarded;
begin
    Guarded (1);
    Guarded (2);
    Check (Caught = 2);
    begin
        Guarded (3);
        raise Program_Error;
    exception
        when Gamma => Put_Line ("unlisted exception propagates");
    end;
    for I in 1 .. 2 loop
        begin
            if I = 1 then
                raise Constraint_Error;
            else
                raise Program_Error;
            end if;
        exception
            when Constraint_Error | Program_Error => Caught := Caught + 1;
        end;
    end loop;
    Check (Caught = 4);
    begin
        raise Gamma;
    exception
        when Alpha | Beta => Put_Line ("MISSED choice list is too wide");
        when others => Put_Line ("others follows a choice list");
    end;
    begin
        begin
            raise Beta;
        exception
            when Alpha | Beta => raise;
        end;
        raise Program_Error;
    exception
        when Beta => Put_Line ("re-raise from a choice list keeps Beta");
    end;
    Put_Line ("handlerchoices: passed");
end HandlerChoices;
