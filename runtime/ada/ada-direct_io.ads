-- Files whose elements can be read and written in any order.  Every element of
-- a direct file is the same width, so an element number is all it takes to say
-- where one sits.

with Ada.IO_Exceptions;
with System;

generic
    type Element_Type is private;

package Ada.Direct_IO is

    -- A file is a handle the run time hands back, wrapped in a record so that
    -- an uninitialised File_Type starts out null and Is_Open can say so.
    type File_Handle is access Integer;

    type File_Type is
    record
        Handle : File_Handle;
    end record;

    type File_Mode is (In_File, Inout_File, Out_File);

    subtype Count is Integer range 0 .. 2147483647;
    subtype Positive_Count is Integer range 1 .. 2147483647;

    procedure Create (File : in out File_Type;
                      Mode : in File_Mode := Inout_File;
                      Name : in String := "";
                      Form : in String := "");
    pragma Import (C, Create, "__ada_direct_create");

    procedure Open (File : in out File_Type;
                    Mode : in File_Mode;
                    Name : in String;
                    Form : in String := "");
    pragma Import (C, Open, "__ada_direct_open");

    procedure Close (File : in out File_Type);
    pragma Import (C, Close, "__ada_file_close");

    procedure Delete (File : in out File_Type);
    pragma Import (C, Delete, "__ada_file_delete");

    procedure Reset (File : in out File_Type; Mode : in File_Mode);
    pragma Import (C, Reset, "__ada_direct_reset");

    procedure Reset (File : in out File_Type);
    pragma Import (C, Reset, "__ada_direct_reset_same");

    function Is_Open (File : in File_Type) return Boolean;
    pragma Import (C, Is_Open, "__ada_file_is_open");

    function Mode (File : in File_Type) return File_Mode;
    pragma Import (C, Mode, "__ada_direct_mode");

    function Name (File : in File_Type) return String;
    pragma Import (C, Name, "__ada_file_name");

    function Form (File : in File_Type) return String;
    pragma Import (C, Form, "__ada_file_form");

    function End_Of_File (File : in File_Type) return Boolean;
    pragma Import (C, End_Of_File, "__ada_file_end_of_file");

    -- Reading and writing without an element number carry on from wherever the
    -- file is.
    procedure Read (File : in File_Type; Item : out Element_Type; From : in Positive_Count);
    procedure Read (File : in File_Type; Item : out Element_Type);

    procedure Write (File : in File_Type; Item : in Element_Type; To : in Positive_Count);
    procedure Write (File : in File_Type; Item : in Element_Type);

    procedure Set_Index (File : in File_Type; To : in Positive_Count);
    function Index (File : in File_Type) return Positive_Count;
    function Size (File : in File_Type) return Count;

    Status_Error : exception renames Ada.IO_Exceptions.Status_Error;
    Mode_Error   : exception renames Ada.IO_Exceptions.Mode_Error;
    Name_Error   : exception renames Ada.IO_Exceptions.Name_Error;
    Use_Error    : exception renames Ada.IO_Exceptions.Use_Error;
    Device_Error : exception renames Ada.IO_Exceptions.Device_Error;
    End_Error    : exception renames Ada.IO_Exceptions.End_Error;
    Data_Error   : exception renames Ada.IO_Exceptions.Data_Error;

end Ada.Direct_IO;
