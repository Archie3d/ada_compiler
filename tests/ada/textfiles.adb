with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure TextFiles is

    Path : constant String (1 .. 13) := "textfiles.tmp";

    Data  : File_Type;
    Line  : String (1 .. 40);
    Last  : Natural;
    Value : Integer;
    Count : Integer := 0;

begin
    Put_Line ("Writing");
    Create (Data, Out_File, Path);
    Put_Line (Data, "first line");
    Put_Line (Data, "second line");
    Put (Data, "count:");
    Put (Data, 42, 4);
    New_Line (Data);
    Close (Data);

    Put_Line ("Reading");
    Open (Data, In_File, Path);
    while not End_Of_File (Data) loop
        Get_Line (Data, Line, Last);
        Count := Count + 1;
        Put (Count, 2);
        Put (": ");
        Put_Line (Line (1 .. Last));
    end loop;
    Close (Data);

    Put_Line ("Fields");
    Open (Data, In_File, Path);
    Skip_Line (Data, 2);
    declare
        Label : String (1 .. 6);
    begin
        Get_Line (Data, Label, Last);
        Put_Line ("dropped " & Label);
    end;
    Close (Data);

    Put_Line ("Numbers");
    Create (Data, Out_File, Path);
    Put (Data, 7, 0);
    New_Line (Data);
    Put (Data, 255, 0, 16);
    New_Line (Data);
    Close (Data);

    Open (Data, In_File, Path);
    Get (Data, Value);
    Put (Value, 4);
    New_Line;
    Close (Data);

    Put_Line ("Queries");
    Open (Data, In_File, Path);
    if Is_Open (Data) then
        Put_Line ("open " & Name (Data));
    end if;
    if Mode (Data) = In_File then
        Put_Line ("mode in_file");
    end if;
    Close (Data);
    if not Is_Open (Data) then
        Put_Line ("closed");
    end if;

    Put_Line ("Redirection");
    Create (Data, Out_File, Path);
    Set_Output (Data);
    Put_Line ("this goes to the file");
    Set_Output (Standard_Output);
    Close (Data);

    Open (Data, In_File, Path);
    Get_Line (Data, Line, Last);
    Put_Line ("recovered " & Line (1 .. Last));
    Delete (Data);

    Put_Line ("Errors");
    begin
        Open (Data, In_File, "no-such-file.tmp");
        Put_Line ("unexpectedly opened");
    exception
        when Name_Error =>
            Put_Line ("name_error");
    end;

    begin
        Close (Data);
    exception
        when Status_Error =>
            Put_Line ("status_error");
    end;

    begin
        Create (Data, In_File, Path);
        Put_Line (Data, "not writable");
    exception
        when Mode_Error =>
            Put_Line ("mode_error");
    end;
    Delete (Data);
end TextFiles;
