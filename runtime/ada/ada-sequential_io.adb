package body Ada.Sequential_IO is

    -- One entry point serves every instantiation, because an element goes to
    -- and from the run time as an address and a width rather than as a value
    -- of a type the run time would have to know.
    procedure Read_Bytes (File : in File_Type; Item : in System.Address; Size : in Integer);
    pragma Import (C, Read_Bytes, "__ada_read_element");

    procedure Write_Bytes (File : in File_Type; Item : in System.Address; Size : in Integer);
    pragma Import (C, Write_Bytes, "__ada_write_element");

    Element_Size : constant Integer := Element_Type'Size / System.Storage_Unit;

    procedure Read (File : in File_Type; Item : out Element_Type) is
    begin
        Read_Bytes (File, Item'Address, Element_Size);
    end Read;

    procedure Write (File : in File_Type; Item : in Element_Type) is
    begin
        Write_Bytes (File, Item'Address, Element_Size);
    end Write;

end Ada.Sequential_IO;
