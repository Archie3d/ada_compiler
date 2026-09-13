package body Ada.Streams.Stream_IO is
    procedure Read (File : in File_Type;
                    Item : out Stream_Element_Array;
                    Last : out Stream_Element_Offset) is
        procedure Read_Elements (File : in File_Type;
                                 Item : out Stream_Element_Array;
                                 First : in Stream_Element_Offset;
                                 Last : out Stream_Element_Offset);
        pragma Import (C, Read_Elements, "__ada_stream_elements_read");
    begin
        Read_Elements (File, Item, Item'First, Last);
    end Read;
end Ada.Streams.Stream_IO;
