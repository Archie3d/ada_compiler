procedure GenericErrors is

   generic
      type Element is private;
      Size : Integer;
   package Boxes is
      function Room return Integer;
   end Boxes;

   package body Boxes is
      function Room return Integer is
      begin
         return Size;
      end Room;
   end Boxes;

   generic
      type Item is private;
   procedure Touch (Value : in out Item);

   procedure Touch (Value : in out Item) is
   begin
      null;
   end Touch;

   Width : Integer := 3;

   package Local_Box is new Boxes (Integer, 4);
   procedure Bad_Actual is new Touch (Width);
   procedure Not_Generic is new Touch;
   procedure Unknown is new Missing (Integer);

begin
   null;
end GenericErrors;
