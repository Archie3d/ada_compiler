with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Composite is

    type Colour is (Red, Green, Blue);

    type Vector is array (1 .. 5) of Integer;

    type Point is record
        X : Integer;
        Y : Integer;
    end record;

    V     : Vector := (1, 2, 3, 4, 5);
    W     : Vector := (others => 7);
    P     : Point := (X => 3, Y => 4);
    Name  : String (1 .. 3) := "ada";
    Shade : Colour := Green;
    Total : Integer := 0;

begin
    for I in V'Range loop
        Total := Total + V (I);
    end loop;
    Put ("sum:");
    Put (Total, 4);
    New_Line;

    Put ("bounds:");
    Put (V'First, 3);
    Put (V'Last, 3);
    Put (V'Length, 3);
    New_Line;

    V (2) := 20;
    Put ("elem:");
    Put (V (2), 4);
    Put (W (3), 4);
    New_Line;

    Put ("point:");
    Put (P.X, 3);
    Put (P.Y, 3);
    New_Line;
    P.X := P.X + P.Y;
    Put ("moved:");
    Put (P.X, 4);
    New_Line;

    Put ("colour:");
    Put (Colour'Pos (Shade), 3);
    if Shade = Green then
        Put (" green");
    end if;
    New_Line;

    Put_Line ("greeting: hello " & Name);

    Put ("chars:");
    Put (' ');
    Put (Name (1));
    Put (Name (3));
    New_Line;
end Composite;
