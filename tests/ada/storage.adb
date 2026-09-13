with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Ada.Sequential_IO;
with System;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Storage is

    type Small is range 0 .. 255;
    for Small'Size use 8;

    type Point is
        record
            X : Integer;
            Y : Integer;
        end record;

    type Pair is array (1 .. 2) of Small;

    package Small_IO is new Ada.Sequential_IO (Small);
    package Point_IO is new Ada.Sequential_IO (Point);

    procedure Show (Label : in String; Bits : in Integer) is
    begin
        Put (Label);
        Put (Bits, 4);
        Put (" bits,");
        Put (Bits / System.Storage_Unit, 3);
        Put_Line (" bytes");
    end Show;

    P : Point := (1, 2);
    Q : Point;
    N : Small := 7;
    M : Small;
    File : Small_IO.File_Type;
    Room : Point_IO.File_Type;

begin
    Put_Line ("Sizes");
    Show ("Integer ", Integer'Size);
    Show ("Small   ", Small'Size);
    Show ("Point   ", Point'Size);
    Show ("Pair    ", Pair'Size);
    Show ("Object  ", N'Size);

    Put_Line ("Addresses");
    if P'Address = P'Address then
        Put_Line ("an object keeps its place");
    end if;
    if P'Address /= Q'Address then
        Put_Line ("two objects do not share one");
    end if;

    -- Sequential_IO hands an element to the run time as its address and its
    -- width, so the width settled above is the width written to the file.
    Put_Line ("Round trip");
    Small_IO.Create (File, Small_IO.Out_File, "storage.small");
    Small_IO.Write (File, N);
    Small_IO.Reset (File, Small_IO.In_File);
    Small_IO.Read (File, M);
    Put (Integer (M), 4);
    New_Line;
    Small_IO.Delete (File);

    Point_IO.Create (Room, Point_IO.Out_File, "storage.point");
    Point_IO.Write (Room, P);
    Point_IO.Reset (Room, Point_IO.In_File);
    Point_IO.Read (Room, Q);
    Put (Q.X, 4);
    Put (Q.Y, 4);
    New_Line;
    Point_IO.Delete (Room);
end Storage;
