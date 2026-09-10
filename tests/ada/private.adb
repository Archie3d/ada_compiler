-- A private type from the outside: the name, the operations and the deferred
-- constant are all there is to work with, and that is enough.

with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Piles;

use Ada.Text_IO;
use Ada.Integer_Text_IO;
use Piles;

procedure Private_Test is

   P     : Pile := Empty;
   Value : Integer;

begin
   Put ("empty "); Put_Line (Boolean'Image (Is_Empty (P)));

   for I in 1 .. 4 loop
      Push (P, I * 10);
   end loop;
   Put ("size"); Put (Size (P), 3); New_Line;

   while Size (P) > 0 loop
      Pop (P, Value);
      Put (Value, 4);
   end loop;
   New_Line;

   -- Assignment and equality come with a private type that is not limited, so
   -- a pile can be copied and compared without knowing anything about it.
   P := Empty;
   Put ("empty again "); Put_Line (Boolean'Image (P = Empty));

   -- The package raises its own exceptions rather than letting a caller reach
   -- past the end of something it cannot see.
   begin
      Pop (P, Value);
      Put_Line ("unreachable");
   exception
      when Underflow =>
         Put_Line ("underflow");
   end;

   begin
      for I in 1 .. Depth + 1 loop
         Push (P, I);
      end loop;
      Put_Line ("unreachable");
   exception
      when Overflow =>
         Put_Line ("overflow");
   end;

end Private_Test;
