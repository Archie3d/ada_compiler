with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Ada.Streams;
with Ada.Streams.Stream_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;
use Ada.Streams;
use Ada.Streams.Stream_IO;

procedure StreamIO is

    type Point is record
        X : Integer;
        Y : Integer;
    end record;

    type Triple is array (1 .. 3) of Integer;

    Path : constant String (1 .. 12) := "streamio.tmp";

    Archive : Ada.Streams.Stream_IO.File_Type;
    Channel : Stream_Access;

    Place  : Point;
    Corner : Triple;
    Count  : Integer;
    Letter : Character;
    Label  : String (1 .. 5);

    Bytes : Stream_Element_Array (1 .. 8);
    Last  : Stream_Element_Offset;

begin
    Put_Line ("Writing");
    Create (Archive, Ada.Streams.Stream_IO.Out_File, Path);
    Channel := Stream (Archive);
    Integer'Write (Channel, 1234);
    Character'Write (Channel, 'Z');
    Point'Write (Channel, (X => 7, Y => 9));
    Triple'Write (Channel, (10, 20, 30));
    String'Output (Channel, "hello");
    Put (Integer (Size (Archive)), 4);
    New_Line;
    Close (Archive);

    Put_Line ("Reading");
    Open (Archive, Ada.Streams.Stream_IO.In_File, Path);
    Channel := Stream (Archive);
    Integer'Read (Channel, Count);
    Character'Read (Channel, Letter);
    Point'Read (Channel, Place);
    Triple'Read (Channel, Corner);
    Put (Count, 6);
    Put (Letter);
    Put (Place.X, 3);
    Put (Place.Y, 3);
    Put (Corner (1), 4);
    Put (Corner (3), 4);
    New_Line;
    Put_Line (String'Input (Channel));
    Close (Archive);

    Put_Line ("Positions");
    Open (Archive, Ada.Streams.Stream_IO.In_File, Path);
    Set_Index (Archive, 5);
    Channel := Stream (Archive);
    Character'Read (Channel, Letter);
    Put (Letter);
    Put (Integer (Index (Archive)), 3);
    New_Line;
    Close (Archive);

    Put_Line ("Elements");
    Open (Archive, Ada.Streams.Stream_IO.In_File, Path);
    Read (Archive, Bytes, Last);
    Put (Integer (Last), 3);
    Put (Integer (Bytes (1)), 4);
    Put (Integer (Bytes (5)), 4);
    New_Line;
    Close (Archive);

    Put_Line ("Copying");
    Create (Archive, Ada.Streams.Stream_IO.Out_File, Path);
    Write (Archive, Bytes (1 .. 4));
    Put (Integer (Size (Archive)), 3);
    New_Line;

    Put_Line ("Errors");
    begin
        Channel := Stream (Archive);
        Integer'Read (Channel, Count);
    exception
        when Mode_Error =>
            Put_Line ("mode_error");
    end;
    Delete (Archive);

    Label := "done ";
    Put_Line (Label);
end StreamIO;
