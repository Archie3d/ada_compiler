with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Ada.Direct_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure DirectIO is

    type Slot is record
        Key   : Integer;
        Score : Integer;
    end record;

    package Slot_IO is new Ada.Direct_IO (Slot);
    use Slot_IO;

    Path : constant String (1 .. 12) := "directio.tmp";

    Table : Slot_IO.File_Type;
    Entry_Value : Slot;

begin
    Put_Line ("Filling");
    Create (Table, Slot_IO.Inout_File, Path);
    for I in 1 .. 5 loop
        Write (Table, (Key => I, Score => I * 10));
    end loop;
    Put (Integer (Size (Table)), 3);
    New_Line;

    Put_Line ("Random access");
    Read (Table, Entry_Value, 4);
    Put (Entry_Value.Key, 3);
    Put (Entry_Value.Score, 5);
    New_Line;

    Read (Table, Entry_Value, 1);
    Put (Entry_Value.Key, 3);
    Put (Entry_Value.Score, 5);
    New_Line;

    Put_Line ("Rewriting");
    Write (Table, (Key => 99, Score => 999), 2);
    Read (Table, Entry_Value, 2);
    Put (Entry_Value.Key, 4);
    Put (Entry_Value.Score, 5);
    New_Line;

    Put_Line ("Walking");
    Set_Index (Table, 1);
    while Index (Table) <= Size (Table) loop
        Read (Table, Entry_Value);
        Put (Entry_Value.Score, 5);
    end loop;
    New_Line;

    Put_Line ("Modes");
    if Mode (Table) = Slot_IO.Inout_File then
        Put_Line ("inout_file");
    end if;
    Close (Table);

    Open (Table, Slot_IO.In_File, Path);
    if Mode (Table) = Slot_IO.In_File then
        Put_Line ("in_file");
    end if;
    begin
        Write (Table, (Key => 0, Score => 0), 1);
    exception
        when Mode_Error =>
            Put_Line ("mode_error");
    end;

    Put_Line ("Beyond the end");
    begin
        Read (Table, Entry_Value, 99);
    exception
        when End_Error =>
            Put_Line ("end_error");
    end;
    Delete (Table);
end DirectIO;
