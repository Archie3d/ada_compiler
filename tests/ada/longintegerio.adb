with Ada.Text_IO; use Ada.Text_IO;
with Ada.Text_IO.Integer_IO;
procedure LongIntegerIO is
    package Wide_IO is new Ada.Text_IO.Integer_IO (Long_Integer);
    File : File_Type;
    Value : Long_Integer;
begin
    Wide_IO.Put (Long_Integer'First, 0);
    New_Line;
    Wide_IO.Put (Long_Integer'Last, 0, 16);
    New_Line;
    Create (File, Out_File, "wide.txt");
    Wide_IO.Put (File, 4294967296, 0);
    New_Line (File);
    Wide_IO.Put (File, Long_Integer'First, 0);
    Close (File);
    Open (File, In_File, "wide.txt");
    Wide_IO.Get (File, Value);
    Put_Line (Long_Integer'Image (Value));
    Wide_IO.Get (File, Value);
    Put_Line (Long_Integer'Image (Value));
    Delete (File);
end LongIntegerIO;
