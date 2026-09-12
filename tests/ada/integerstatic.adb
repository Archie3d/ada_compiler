with Ada.Text_IO; use Ada.Text_IO;

procedure integerstatic is
   Remainder_Value : constant := (-7) rem 3;
   Modulo_Value : constant := (-7) mod 3;
   Result : Long_Integer;
   Exponent : Integer := -1;
   Two : Long_Integer := 2;
begin
   Put_Line (Integer'Image (Integer (Remainder_Value)));
   Put_Line (Integer'Image (Integer (Modulo_Value)));
   begin
      Result := 9223372036854775807 + 1;
      Put_Line ("MISSED static overflow");
   exception
      when Constraint_Error => Put_Line ("caught static overflow");
   end;
   begin
      Result := Two ** Exponent;
      Put_Line ("MISSED negative exponent");
   exception
      when Constraint_Error => Put_Line ("caught negative exponent");
   end;
   Put_Line (Long_Integer'Image (Two ** 10));
end integerstatic;
