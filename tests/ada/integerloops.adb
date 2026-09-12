with Ada.Text_IO; use Ada.Text_IO;

procedure integerloops is
   Count : Integer := 0;
begin
   for I in Integer'Last .. Integer'Last loop
      Count := Count + 1;
      exit when Count > 2;
   end loop;
   Put_Line (Integer'Image (Count));
   Count := 0;
   for I in reverse Long_Integer'First .. Long_Integer'First loop
      Count := Count + 1;
      exit when Count > 2;
   end loop;
   Put_Line (Integer'Image (Count));
   for I in Long_Integer range 4294967296 .. 4294967298 loop
      Put_Line (Long_Integer'Image (I));
   end loop;
end integerloops;
