with Ada.Text_IO;
use Ada.Text_IO;

procedure Checks is

   subtype Small is Integer range 1 .. 10;

   Overflow : exception;

   procedure Store (Value : Integer) is
      Slot : Small;
   begin
      Slot := Value;
      Put_Line ("stored" & Integer'Image (Slot));
   exception
      when Constraint_Error =>
         Put_Line ("out of range" & Integer'Image (Value));
   end Store;

   function Checked (Value : Integer) return Integer is
   begin
      if Value > 100 then
         raise Overflow;
      end if;
      return Value * 2;
   end Checked;

begin
   Store (5);
   Store (50);

   Put_Line ("doubled" & Integer'Image (Checked (21)));

   begin
      Put_Line ("doubled" & Integer'Image (Checked (500)));
   exception
      when Overflow =>
         Put_Line ("overflow caught");
   end;

   declare
      X : Small := 3;
   begin
      X := X * 5;
      Put_Line ("not reached");
   exception
      when others =>
         Put_Line ("others caught");
   end;

   Put_Line ("done");
end Checks;
