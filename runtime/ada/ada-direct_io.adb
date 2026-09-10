package body Ada.Direct_IO is

    -- One entry point serves every instantiation, because an element goes to
    -- and from the run time as an address and a width rather than as a value
    -- of a type the run time would have to know.  An element number of zero
    -- means wherever the file already is.
    procedure Read_Bytes (File  : in File_Type;
                          Item  : in System.Address;
                          Size  : in Integer;
                          Index : in Integer);
    pragma Import (C, Read_Bytes, "__ada_direct_read_at");

    procedure Write_Bytes (File  : in File_Type;
                           Item  : in System.Address;
                           Size  : in Integer;
                           Index : in Integer);
    pragma Import (C, Write_Bytes, "__ada_direct_write_at");

    procedure Move_To (File : in File_Type; To : in Integer; Size : in Integer);
    pragma Import (C, Move_To, "__ada_direct_set_index");

    function Index_Of (File : in File_Type; Size : in Integer) return Integer;
    pragma Import (C, Index_Of, "__ada_direct_index");

    function Size_Of (File : in File_Type; Size : in Integer) return Integer;
    pragma Import (C, Size_Of, "__ada_direct_size");

    Element_Size : constant Integer := Element_Type'Size / System.Storage_Unit;

    procedure Read (File : in File_Type; Item : out Element_Type; From : in Positive_Count) is
    begin
        Read_Bytes (File, Item'Address, Element_Size, From);
    end Read;

    procedure Read (File : in File_Type; Item : out Element_Type) is
    begin
        Read_Bytes (File, Item'Address, Element_Size, 0);
    end Read;

    procedure Write (File : in File_Type; Item : in Element_Type; To : in Positive_Count) is
    begin
        Write_Bytes (File, Item'Address, Element_Size, To);
    end Write;

    procedure Write (File : in File_Type; Item : in Element_Type) is
    begin
        Write_Bytes (File, Item'Address, Element_Size, 0);
    end Write;

    procedure Set_Index (File : in File_Type; To : in Positive_Count) is
    begin
        Move_To (File, To, Element_Size);
    end Set_Index;

    function Index (File : in File_Type) return Positive_Count is
    begin
        return Index_Of (File, Element_Size);
    end Index;

    function Size (File : in File_Type) return Count is
    begin
        return Size_Of (File, Element_Size);
    end Size;

end Ada.Direct_IO;
