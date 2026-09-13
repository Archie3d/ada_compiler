-- Line reads handle empty, split and unterminated lines with input redirection.
with Ada.Text_IO; use Ada.Text_IO;
procedure TextFileEdges is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Data : File_Type;
    Buffer : String (1 .. 3);
    Last : Natural;
begin
    Create (Data, Out_File, "textfileedges.tmp");
    Close (Data);
    Open (Data, In_File, "textfileedges.tmp");
    Check (End_Of_File (Data));
    Delete (Data);
    Create (Data, Out_File, "textfileedges.tmp");
    New_Line (Data);
    Put_Line (Data, "abcdef");
    Put (Data, "xy");
    Close (Data);
    Open (Data, In_File, "textfileedges.tmp");
    Set_Input (Data);
    Check (Name (Current_Input) = Name (Data));
    Get_Line (Buffer, Last);
    Check (Last = 0);
    Get_Line (Buffer, Last);
    Check (Last = 3 and Buffer = "abc");
    Get_Line (Buffer, Last);
    Check (Last = 3 and Buffer = "def");
    Get_Line (Buffer, Last);
    Check (Last = 2 and Buffer (1 .. Last) = "xy");
    Check (End_Of_File);
    Set_Input (Standard_Input);
    Delete (Data);
    Put_Line ("textfileedges: passed");
end TextFileEdges;
