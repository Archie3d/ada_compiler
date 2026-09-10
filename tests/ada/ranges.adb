with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Ranges is

   type Level is range 1 .. 10;
   subtype Low_Level is Level range 1 .. 3;
   type Offset is range -5 .. 5;

   procedure Take (Value : Low_Level) is
   begin
      Put ("took");
      Put (Integer (Value), 3);
      New_Line;
   end Take;

   function Doubled (Value : Level) return Level is
   begin
      return Value * 2;
   end Doubled;

   Current : Level := 4;
   Shift   : Offset := -5;

begin
   Put ("bounds");
   Put (Integer (Level'First), 3);
   Put (Integer (Level'Last), 3);
   Put (Integer (Offset'First), 3);
   Put (Integer (Low_Level'Last), 3);
   New_Line;

   Put_Line ("image:" & Level'Image (Current));

   for Step in Level range 1 .. 4 loop
      Put (Integer (Step), 3);
   end loop;
   New_Line;

   Current := Doubled (Current);
   Put ("doubled");
   Put (Integer (Current), 3);
   New_Line;

   Shift := Shift + 4;
   Put ("shift");
   Put (Integer (Shift), 3);
   New_Line;

   Take (2);

   begin
      Take (7);
      Put_Line ("no check on the argument");
   exception
      when Constraint_Error =>
         Put_Line ("argument out of range");
   end;

   begin
      Current := Doubled (Current);
      Put_Line ("no check on the result");
   exception
      when Constraint_Error =>
         Put_Line ("result out of range");
   end;

   begin
      Shift := Shift * 9;
      Put_Line ("no check on the assignment");
   exception
      when Constraint_Error =>
         Put_Line ("assignment out of range");
   end;
end Ranges;
