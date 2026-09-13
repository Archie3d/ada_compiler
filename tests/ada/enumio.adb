with Ada.Text_IO;
with Ada.Text_IO.Enumeration_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure EnumIO is

    type Day is (Monday, Tuesday, Wednesday, Thursday, Friday);
    type Colour is (Red, Green, Blue);
    subtype Weekend_Eve is Day range Thursday .. Friday;

    package Day_IO is new Ada.Text_IO.Enumeration_IO (Day);
    package Colour_IO is new Ada.Text_IO.Enumeration_IO (Colour);
    package Flag_IO is new Ada.Text_IO.Enumeration_IO (Boolean);

    use Day_IO, Colour_IO, Flag_IO;

    Path : constant String (1 .. 10) := "enumio.tmp";

    Today : Day := Wednesday;
    Shade : Colour := Green;
    Later : Weekend_Eve := Friday;

    Data : File_Type;

begin
    Put ("plain  ");
    Put (Today);
    Put (Shade);
    Put (True);
    New_Line;

    --  Ada writes a literal in upper case unless asked otherwise, and fills a
    --  wider field out on the right.
    Put ("cased |");
    Put (Today, 0, Lower_Case);
    Put ("|");
    Put (Today, 12);
    Put ("|");
    Put (Shade, 8, Lower_Case);
    Put_Line ("|");

    Put_Line ("image " & Day'Image (Today) & Colour'Image (Shade)
                & Boolean'Image (Today < Later));

    Put ("subtype");
    Put (Later, 10);
    Put (Integer (Day'Pos (Later)), 3);
    New_Line;

    Create (Data, Out_File, Path);
    Put (Data, Today);
    New_Line (Data);
    Put (Data, Shade, 0, Lower_Case);
    Put_Line (Data, " thursday");
    Close (Data);

    Open (Data, In_File, Path);
    declare
        First  : Day;
        Second : Colour;
        Third  : Day;
    begin
        Get (Data, First);
        Get (Data, Second);
        Get (Data, Third);
        Put ("read   ");
        Put (First);
        Put (Second);
        Put (Third);
        New_Line;
    end;
    Close (Data);

    --  A word that is not a literal of the type is Data_Error.
    Create (Data, Out_File, Path);
    Put_Line (Data, "saturday");
    Close (Data);

    Open (Data, In_File, Path);
    declare
        Unknown : Day;
    begin
        Get (Data, Unknown);
        Put_Line ("no check on the word read");
    exception
        when Data_Error =>
            Put_Line ("not a literal of the type");
    end;
    Delete (Data);
end EnumIO;
