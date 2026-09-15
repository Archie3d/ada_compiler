-- Reverse iteration over a type or subtype name visits every value, last first.
with Ada.Text_IO; use Ada.Text_IO;
procedure ReverseLoops is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    type Colour is (Red, Green, Blue);
    type Solo is (Only);
    subtype Warm is Colour range Red .. Green;
    subtype Digit is Character range '0' .. '2';
    Total : Integer := 0;
    Count : Integer := 0;
begin
    for C in reverse Colour loop
        Total := Total * 10 + Colour'Pos (C);
    end loop;
    Check (Total = 210);
    Total := 0;
    for C in Colour loop
        Total := Total * 10 + Colour'Pos (C);
    end loop;
    Check (Total = 12);
    Total := 0;
    for C in reverse Warm loop
        Total := Total * 10 + Colour'Pos (C);
    end loop;
    Check (Total = 10);
    Total := 0;
    for B in reverse Boolean loop
        Total := Total * 10 + Boolean'Pos (B);
    end loop;
    Check (Total = 10);
    for S in reverse Solo loop
        Count := Count + 1;
    end loop;
    Check (Count = 1);
    Total := 0;
    for D in reverse Digit loop
        Total := Total * 10 + Character'Pos (D) - Character'Pos ('0');
    end loop;
    Check (Total = 210);
    Count := 0;
    for C in reverse Colour loop
        for D in reverse Colour loop
            Count := Count + 1;
        end loop;
    end loop;
    Check (Count = 9);
    Put_Line ("reverseloops: passed");
end ReverseLoops;
