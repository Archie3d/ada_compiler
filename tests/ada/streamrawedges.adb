-- Last denotes the final filled index, even when the buffer starts above 1.
with Ada.Text_IO; use Ada.Text_IO;
with Ada.Streams;
with Ada.Streams.Stream_IO;
procedure StreamRawEdges is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Data : Ada.Streams.Stream_IO.File_Type;
    Channel : Ada.Streams.Stream_IO.Stream_Access;
    Bytes : Ada.Streams.Stream_Element_Array (5 .. 8) := (11, 22, 33, 44);
    Empty : Ada.Streams.Stream_Element_Array (8 .. 7);
    Last : Ada.Streams.Stream_Element_Offset;
    Value : Integer;
begin
    Ada.Streams.Stream_IO.Create (Data, Ada.Streams.Stream_IO.Out_File, "streamrawedges.tmp");
    Ada.Streams.Stream_IO.Write (Data, Bytes (5 .. 6));
    Ada.Streams.Stream_IO.Close (Data);
    Ada.Streams.Stream_IO.Open (Data, Ada.Streams.Stream_IO.In_File, "streamrawedges.tmp");
    Ada.Streams.Stream_IO.Read (Data, Bytes, Last);
    Ada.Streams.Stream_IO.Set_Index (Data, 1);
    Channel := Ada.Streams.Stream_IO.Stream (Data);
    begin
        Integer'Read (Channel, Value);
        raise Program_Error;
    exception
        when Ada.Streams.Stream_IO.End_Error => null;
    end;
    Check (Last = 6 and Bytes (5) = 11 and Bytes (6) = 22);
    Ada.Streams.Stream_IO.Read (Data, Bytes, Last);
    Check (Last = 4);
    Ada.Streams.Stream_IO.Set_Index (Data, 1);
    Ada.Streams.Stream_IO.Read (Data, Empty, Last);
    Check (Last = 7 and Ada.Streams.Stream_IO.Index (Data) = 1);
    Ada.Streams.Stream_IO.Read (Data, Bytes (6 .. 7), Last);
    Check (Last = 7 and Bytes (6) = 11 and Bytes (7) = 22);
    Ada.Streams.Stream_IO.Delete (Data);
    Put_Line ("streamrawedges: passed");
end StreamRawEdges;
