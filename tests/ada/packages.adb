with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Counters;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Packages is
begin
   Put ("start:");
   Put (Counters.Value, 5);
   New_Line;

   Counters.Bump (5);
   Counters.Bump (7);
   Put ("bumped:");
   Put (Counters.Value, 5);
   New_Line;

   Counters.Reset;
   Put ("reset:");
   Put (Counters.Value, 5);
   New_Line;
end Packages;
