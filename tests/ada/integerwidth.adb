with Ada.Text_IO; use Ada.Text_IO;

procedure integerwidth is
   subtype Small_Long is Long_Integer range -5 .. 5;
   type Wide is range -5000000000 .. 5000000000;
   type Tiny is range -128 .. 127;
   for Tiny'Size use 8;
   subtype Tiny_Sub is Tiny range -10 .. 10;
   X : Long_Integer := 4294967296;
   Y : Wide := 5000000000;
   N : Tiny_Sub := -7;
   function Echo (Value : Long_Integer) return Long_Integer is
   begin
      return Value;
   end Echo;
begin
   Put_Line (Long_Integer'Image (Echo (X)));
   Put_Line (Wide'Image (Y));
   Put_Line (Integer'Image (Long_Integer'Size));
   Put_Line (Integer'Image (Long_Integer'Width));
   Put_Line (Integer'Image (Small_Long'Size));
   Put_Line (Integer'Image (Tiny_Sub'Size));
   Put_Line (Tiny_Sub'Image (N));
   Put_Line (Long_Integer'Image (Long_Integer'Value ("-9223372036854775808")));
   Put_Line (Long_Integer'Image (Long_Integer'Value ("9223372036854775807")));
   Put_Line (Integer'Image (Integer'Value ("-2147483648")));
   begin
      X := Long_Integer'Value ("9223372036854775808");
      Put_Line ("MISSED Value overflow");
   exception
      when Constraint_Error => Put_Line ("caught Value overflow");
   end;
   Put_Line (Long_Integer'Image (Long_Integer'Val (4294967296)));
   begin
      X := Long_Integer'Succ (Long_Integer'Last);
      Put_Line ("MISSED successor overflow");
   exception
      when Constraint_Error => Put_Line ("caught successor overflow");
   end;
end integerwidth;
