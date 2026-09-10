package Counters is

   Start_Value : constant Integer := 100;

   procedure Reset;
   procedure Bump (By : Integer);
   function Value return Integer;

end Counters;
