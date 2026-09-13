with Ada.Text_IO;
with Ada.Text_IO.Integer_IO;
with Ada.Text_IO.Float_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;

procedure TypeIO is

    type Level is range 1 .. 100;
    type Offset is range -5000 .. 5000;
    type Small is digits 6 range -100.0 .. 100.0;
    type Wide is digits 12;

    package Level_IO is new Ada.Text_IO.Integer_IO (Level);
    package Offset_IO is new Ada.Text_IO.Integer_IO (Offset);
    package Small_IO is new Ada.Text_IO.Float_IO (Small);
    package Wide_IO is new Ada.Text_IO.Float_IO (Wide);

    use Level_IO, Offset_IO, Small_IO, Wide_IO;

    Path : constant String (1 .. 10) := "typeio.tmp";

    Step  : Level := 42;
    Shift : Offset := -1234;
    Size  : Small := 2.5;
    Exact : Wide := 1.5;

    Data : File_Type;

begin
    --  A profile names the type it was instantiated with, so no conversion to
    --  Integer stands between the value and the page.
    Put ("values");
    Put (Step, 6);
    Put (Shift, 8);
    Put (Size, 4, 2, 0);
    Put (Exact, 4, 3, 0);
    New_Line;

    --  Ada takes the default width from the type: Level'Width is three and
    --  Offset'Width is five, where Integer'Width is eleven.
    Put ("widths|");
    Put (Step);
    Put ("|");
    Put (Shift);
    Put ("|");
    Ada.Integer_Text_IO.Put (Integer (Step));
    Put_Line ("|");

    --  Aft follows from the digits of the type, so the same value shows a
    --  different number of places through each instance.
    Put ("digits ");
    Put (Size);
    Put (Exact);
    New_Line;

    Put ("based  ");
    Put (Step, 8, 16);
    Put (Step, 12, 2);
    New_Line;

    Create (Data, Out_File, Path);
    Put (Data, Step, 4);
    Put (Data, Shift, 6);
    New_Line (Data);
    Put (Data, Size, 3, 2, 0);
    New_Line (Data);
    Close (Data);

    Open (Data, In_File, Path);
    declare
        Back  : Level;
        Where : Offset;
        Value : Small;
    begin
        Get (Data, Back);
        Get (Data, Where);
        Get (Data, Value);
        Put ("read   ");
        Put (Back, 6);
        Put (Where, 8);
        Put (Value, 4, 2, 0);
        New_Line;
    end;
    Close (Data);

    --  A number the type cannot hold is Data_Error, not Constraint_Error.
    Create (Data, Out_File, Path);
    Put_Line (Data, "500");
    Close (Data);

    Open (Data, In_File, Path);
    declare
        Back : Level;
    begin
        Get (Data, Back);
        Put_Line ("no check on the value read");
    exception
        when Data_Error =>
            Put_Line ("value out of range");
    end;
    Delete (Data);
end TypeIO;
