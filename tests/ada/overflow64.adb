with Ada.Text_IO; use Ada.Text_IO;

procedure overflow64 is
   Hi : Long_Integer := Long_Integer'Last;
   Lo : Long_Integer := Long_Integer'First;
   One : Long_Integer := 1;
   Two : Long_Integer := 2;
   Minus_One : Long_Integer := -1;
   Exponent : Integer := 63;
   Result : Long_Integer;
begin
   begin
      Result := Hi + One;
      Put_Line ("MISSED add");
   exception
      when Constraint_Error => Put_Line ("caught add");
   end;
   begin
      Result := Lo - One;
      Put_Line ("MISSED subtract");
   exception
      when Constraint_Error => Put_Line ("caught subtract");
   end;
   begin
      Result := Hi * Two;
      Put_Line ("MISSED multiply");
   exception
      when Constraint_Error => Put_Line ("caught multiply");
   end;
   begin
      Result := Lo / Minus_One;
      Put_Line ("MISSED divide");
   exception
      when Constraint_Error => Put_Line ("caught divide");
   end;
   begin
      Result := -Lo;
      Put_Line ("MISSED negate");
   exception
      when Constraint_Error => Put_Line ("caught negate");
   end;
   begin
      Result := abs Lo;
      Put_Line ("MISSED absolute");
   exception
      when Constraint_Error => Put_Line ("caught absolute");
   end;
   begin
      Result := Two ** Exponent;
      Put_Line ("MISSED power");
   exception
      when Constraint_Error => Put_Line ("caught power");
   end;
end overflow64;
