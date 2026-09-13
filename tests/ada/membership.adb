-- Membership includes endpoints, subtype marks, enumeration and null ranges.
with Ada.Text_IO; use Ada.Text_IO;
procedure Membership is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    subtype Small is Integer range 1 .. 3;
    type Colour is (Red, Green, Blue);
    subtype Early is Colour range Red .. Green;
    Low : Integer := 1;
    High : Integer := 3;
begin
    Check ((Low in Small) and (High in Small));
    Check ((0 not in Small) and (4 not in Small));
    Check (2 in Low .. High and 4 not in Low .. High);
    Check (2 not in High .. Low);
    Check (Red in Red .. Green and Blue not in Red .. Green);
    Check ((Green in Early) and (Blue not in Early));
    Put_Line ("membership: passed");
end Membership;
