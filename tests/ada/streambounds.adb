-- String stream round trips preserve nonstandard and null bounds.
with Ada.Streams;
with Ada.Streams.Stream_IO;
with Ada.Text_IO; use Ada.Text_IO;
procedure StreamBounds is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    Data : Ada.Streams.Stream_IO.File_Type;
    Channel : Ada.Streams.Stream_IO.Stream_Access;
    Text : String (5 .. 7) := "abc";
    Empty : String (8 .. 3);
begin
    Ada.Streams.Stream_IO.Create (Data, Ada.Streams.Stream_IO.Out_File, "streambounds.tmp");
    Channel := Ada.Streams.Stream_IO.Stream (Data);
    String'Output (Channel, Text);
    String'Output (Channel, Empty);
    Ada.Streams.Stream_IO.Close (Data);
    Ada.Streams.Stream_IO.Open (Data, Ada.Streams.Stream_IO.In_File, "streambounds.tmp");
    Channel := Ada.Streams.Stream_IO.Stream (Data);
    declare
        Back : String := String'Input (Channel);
        Null_Back : String := String'Input (Channel);
    begin
        Check (Back = Text and Back'First = 5 and Back'Last = 7);
        Check (Null_Back'Length = 0 and Null_Back'First = 8 and Null_Back'Last = 3);
    end;
    Ada.Streams.Stream_IO.Delete (Data);
    Put_Line ("streambounds: passed");
end StreamBounds;
