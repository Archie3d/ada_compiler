with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Ada.Sequential_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure SeqIO is

    type Reading is record
        Station : Integer;
        Value   : Integer;
    end record;

    package Reading_IO is new Ada.Sequential_IO (Reading);
    package Number_IO is new Ada.Sequential_IO (Element_Type => Integer);

    Path : constant String (1 .. 11) := "seqio.tmp  ";

    Log   : Reading_IO.File_Type;
    Tally : Number_IO.File_Type;
    Item  : Reading;
    Count : Integer;

begin
    Put_Line ("Records");
    Reading_IO.Create (Log, Reading_IO.Out_File, Path (1 .. 9));
    Reading_IO.Write (Log, (Station => 1, Value => 100));
    Reading_IO.Write (Log, (Station => 2, Value => 200));
    Reading_IO.Write (Log, (Station => 3, Value => 300));
    Reading_IO.Close (Log);

    Reading_IO.Open (Log, Reading_IO.In_File, Path (1 .. 9));
    while not Reading_IO.End_Of_File (Log) loop
        Reading_IO.Read (Log, Item);
        Put (Item.Station, 3);
        Put (Item.Value, 6);
        New_Line;
    end loop;

    Put_Line ("Past the end");
    begin
        Reading_IO.Read (Log, Item);
        Put_Line ("unexpectedly read");
    exception
        when Reading_IO.End_Error =>
            Put_Line ("end_error");
    end;
    Reading_IO.Delete (Log);

    Put_Line ("Numbers");
    Number_IO.Create (Tally, Number_IO.Out_File, Path (1 .. 9));
    for I in 1 .. 4 loop
        Number_IO.Write (Tally, I * I);
    end loop;
    Number_IO.Close (Tally);

    Number_IO.Open (Tally, Number_IO.In_File, Path (1 .. 9));
    while not Number_IO.End_Of_File (Tally) loop
        Number_IO.Read (Tally, Count);
        Put (Count, 4);
    end loop;
    New_Line;

    Put_Line ("Wrong mode");
    begin
        Number_IO.Write (Tally, 5);
    exception
        when Number_IO.Mode_Error =>
            Put_Line ("mode_error");
    end;
    Number_IO.Delete (Tally);
end SeqIO;
