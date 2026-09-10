-- Text input and output.  Every operation here is carried out by the C run
-- time, which this specification reaches through pragma Import: the Ada side
-- contributes the profiles, the defaults and the names, and nothing else.

with Ada.IO_Exceptions;

package Ada.Text_IO is

    -- A file is a handle the run time hands back, wrapped in a record so that
    -- an uninitialised File_Type starts out null and Is_Open can say so.
    type File_Handle is access Integer;

    type File_Type is
    record
        Handle : File_Handle;
    end record;

    type File_Mode is (In_File, Out_File, Append_File);

    -- The widths and bases the generic children below take as arguments.
    subtype Field is Integer range 0 .. 2147483647;
    subtype Number_Base is Integer range 2 .. 16;
    type Type_Set is (Lower_Case, Upper_Case);

    -- File management
    -- -----------------------------------------------------------------------

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

    -- Default input and output files
    -- -----------------------------------------------------------------------

    procedure Set_Input (File : in File_Type);
    pragma Import (C, Set_Input, "__ada_set_input");

    procedure Set_Output (File : in File_Type);
    pragma Import (C, Set_Output, "__ada_set_output");

    function Standard_Input return File_Type;
    pragma Import (C, Standard_Input, "__ada_standard_input");

    function Standard_Output return File_Type;
    pragma Import (C, Standard_Output, "__ada_standard_output");

    function Standard_Error return File_Type;
    pragma Import (C, Standard_Error, "__ada_standard_error");

    function Current_Input return File_Type;
    pragma Import (C, Current_Input, "__ada_current_input");

    function Current_Output return File_Type;
    pragma Import (C, Current_Output, "__ada_current_output");

    -- Characters, strings and lines
    -- -----------------------------------------------------------------------
    -- The profiles without a file come first, so that a call naming neither a
    -- file nor anything else settles on the one working through Current_Output.

    procedure Put_Line (Item : in String);
    pragma Import (C, Put_Line, "__ada_put_line");

    procedure Put (Item : in String);
    pragma Import (C, Put, "__ada_put");

    procedure Put (Item : in Character);
    pragma Import (C, Put, "__ada_put_character");

    procedure New_Line (Spacing : in Positive := 1);
    pragma Import (C, New_Line, "__ada_new_line");

    procedure Skip_Line (Spacing : in Positive := 1);
    pragma Import (C, Skip_Line, "__ada_skip_line");

    function End_Of_File return Boolean;
    pragma Import (C, End_Of_File, "__ada_end_of_file");

    function End_Of_Line return Boolean;
    pragma Import (C, End_Of_Line, "__ada_end_of_line");

    procedure Get (Item : out Character);
    pragma Import (C, Get, "__ada_get");

    procedure Get_Line (Item : out String; Last : out Natural);
    pragma Import (C, Get_Line, "__ada_get_line");

    procedure Put_Line (File : in File_Type; Item : in String);
    pragma Import (C, Put_Line, "__ada_text_put_line");

    procedure Put (File : in File_Type; Item : in String);
    pragma Import (C, Put, "__ada_text_put");

    procedure Put (File : in File_Type; Item : in Character);
    pragma Import (C, Put, "__ada_text_put_character");

    procedure New_Line (File : in File_Type; Spacing : in Positive := 1);
    pragma Import (C, New_Line, "__ada_text_new_line");

    procedure Skip_Line (File : in File_Type; Spacing : in Positive := 1);
    pragma Import (C, Skip_Line, "__ada_text_skip_line");

    function End_Of_File (File : in File_Type) return Boolean;
    pragma Import (C, End_Of_File, "__ada_file_end_of_file");

    function End_Of_Line (File : in File_Type) return Boolean;
    pragma Import (C, End_Of_Line, "__ada_text_end_of_line");

    procedure Get (File : in File_Type; Item : out Character);
    pragma Import (C, Get, "__ada_text_get");

    procedure Get_Line (File : in File_Type; Item : out String; Last : out Natural);
    pragma Import (C, Get_Line, "__ada_text_get_line");

    -- The exceptions of Ada.IO_Exceptions under the names Text_IO gives them.
    -- These are renamings, so a handler written for either one catches both.

    Status_Error : exception renames IO_Exceptions.Status_Error;
    Mode_Error   : exception renames IO_Exceptions.Mode_Error;
    Name_Error   : exception renames IO_Exceptions.Name_Error;
    Use_Error    : exception renames IO_Exceptions.Use_Error;
    Device_Error : exception renames IO_Exceptions.Device_Error;
    End_Error    : exception renames IO_Exceptions.End_Error;
    Data_Error   : exception renames IO_Exceptions.Data_Error;
    Layout_Error : exception renames IO_Exceptions.Layout_Error;

end Ada.Text_IO;
