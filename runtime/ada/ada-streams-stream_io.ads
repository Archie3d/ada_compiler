-- A file seen as the run of bytes it really is.  The 'Read, 'Write, 'Input and
-- 'Output attributes put values into such a file and take them back out; this
-- package is what opens one and says where in it to work.

with Ada.IO_Exceptions;

package Ada.Streams.Stream_IO is

    -- A file is a handle the run time hands back, wrapped in a record so that
    -- an uninitialised File_Type starts out null and Is_Open can say so.
    type File_Handle is access Integer;

    type File_Type is
    record
        Handle : File_Handle;
    end record;

    type File_Mode is (In_File, Out_File, Append_File);

    -- Without tagged types a stream is just the file it belongs to, so the
    -- access type points at nothing the language can name.
    type Stream_Access is access Integer;

    subtype Count is Integer range 0 .. 2147483647;
    subtype Positive_Count is Integer range 1 .. 2147483647;

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

    function Stream (File : in File_Type) return Stream_Access;
    pragma Import (C, Stream, "__ada_stream_of");

    procedure Read (File : in File_Type;
                    Item : out Stream_Element_Array;
                    Last : out Stream_Element_Offset);
    pragma Import (C, Read, "__ada_stream_elements_read");

    procedure Write (File : in File_Type; Item : in Stream_Element_Array);
    pragma Import (C, Write, "__ada_stream_elements_write");

    procedure Set_Index (File : in File_Type; To : in Positive_Count);
    pragma Import (C, Set_Index, "__ada_stream_set_index");

    function Index (File : in File_Type) return Positive_Count;
    pragma Import (C, Index, "__ada_stream_index");

    function Size (File : in File_Type) return Count;
    pragma Import (C, Size, "__ada_stream_size");

    Status_Error : exception renames Ada.IO_Exceptions.Status_Error;
    Mode_Error   : exception renames Ada.IO_Exceptions.Mode_Error;
    Name_Error   : exception renames Ada.IO_Exceptions.Name_Error;
    Use_Error    : exception renames Ada.IO_Exceptions.Use_Error;
    Device_Error : exception renames Ada.IO_Exceptions.Device_Error;
    End_Error    : exception renames Ada.IO_Exceptions.End_Error;
    Data_Error   : exception renames Ada.IO_Exceptions.Data_Error;

end Ada.Streams.Stream_IO;
