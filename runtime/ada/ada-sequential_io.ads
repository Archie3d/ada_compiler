-- Files read and written one whole element at a time, in the order they were
-- put there.  The run time moves bytes and knows nothing of the element type;
-- how wide an element is and where one sits is settled here.

with Ada.IO_Exceptions;
with System;

generic
    type Element_Type is private;

package Ada.Sequential_IO is

    -- A file is a handle the run time hands back, wrapped in a record so that
    -- an uninitialised File_Type starts out null and Is_Open can say so.
    type File_Handle is access Integer;

    type File_Type is
    record
        Handle : File_Handle;
    end record;

    type File_Mode is (In_File, Out_File);

    procedure Create (File : in out File_Type;
                      Mode : in File_Mode := Out_File;
                      Name : in String := "";
                      Form : in String := "");
    pragma Import (C, Create, "__ada_create");

    procedure Open (File : in out File_Type;
                    Mode : in File_Mode;
                    Name : in String;
                    Form : in String := "");
    pragma Import (C, Open, "__ada_open");

    procedure Close (File : in out File_Type);
    pragma Import (C, Close, "__ada_file_close");

    procedure Delete (File : in out File_Type);
    pragma Import (C, Delete, "__ada_file_delete");

    procedure Reset (File : in out File_Type; Mode : in File_Mode);
    pragma Import (C, Reset, "__ada_file_reset");

    procedure Reset (File : in out File_Type);
    pragma Import (C, Reset, "__ada_file_reset_same");

    function Is_Open (File : in File_Type) return Boolean;
    pragma Import (C, Is_Open, "__ada_file_is_open");

    function Mode (File : in File_Type) return File_Mode;
    pragma Import (C, Mode, "__ada_file_mode");

    function Name (File : in File_Type) return String;
    pragma Import (C, Name, "__ada_file_name");

    function Form (File : in File_Type) return String;
    pragma Import (C, Form, "__ada_file_form");

    function End_Of_File (File : in File_Type) return Boolean;
    pragma Import (C, End_Of_File, "__ada_file_end_of_file");

    procedure Read (File : in File_Type; Item : out Element_Type);

    procedure Write (File : in File_Type; Item : in Element_Type);

    Status_Error : exception renames Ada.IO_Exceptions.Status_Error;
    Mode_Error   : exception renames Ada.IO_Exceptions.Mode_Error;
    Name_Error   : exception renames Ada.IO_Exceptions.Name_Error;
    Use_Error    : exception renames Ada.IO_Exceptions.Use_Error;
    Device_Error : exception renames Ada.IO_Exceptions.Device_Error;
    End_Error    : exception renames Ada.IO_Exceptions.End_Error;
    Data_Error   : exception renames Ada.IO_Exceptions.Data_Error;

end Ada.Sequential_IO;
