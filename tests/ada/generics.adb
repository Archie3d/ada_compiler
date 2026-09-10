with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Stacks;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Generics is

   package Number_Stack is new Stacks (Integer);
   package Letter_Stack is new Stacks (Element => Character, Capacity => 2);

   generic
      type Item is private;
   procedure Swap (Left, Right : in out Item);

   procedure Swap (Left, Right : in out Item) is
      Saved : Item := Left;
   begin
      Left := Right;
      Right := Saved;
   end Swap;

   procedure Swap_Numbers is new Swap (Integer);
   procedure Swap_Letters is new Swap (Character);

   A : Integer := 1;
   B : Integer := 2;
   X : Character := 'x';
   Y : Character := 'y';

begin
   Put_Line ("Stacks");
   Number_Stack.Push (10);
   Number_Stack.Push (20);
   Number_Stack.Push (30);
   Put (Number_Stack.Depth, 2);
   Put (Number_Stack.Room, 3);
   New_Line;
   Put (Number_Stack.Pop, 4);
   Put (Number_Stack.Pop, 4);
   Put (Number_Stack.Depth, 3);
   New_Line;

   Letter_Stack.Push ('a');
   Letter_Stack.Push ('b');
   Put (Letter_Stack.Pop);
   Put (Letter_Stack.Pop);
   Put (Letter_Stack.Room, 3);
   New_Line;

   Put_Line ("Capacity");
   begin
      Letter_Stack.Push ('c');
      Letter_Stack.Push ('d');
      Letter_Stack.Push ('e');
   exception
      when Constraint_Error =>
         Put_Line ("full");
   end;

   Put_Line ("Swapping");
   Swap_Numbers (A, B);
   Put (A, 3);
   Put (B, 3);
   New_Line;
   Swap_Letters (X, Y);
   Put (X);
   Put (Y);
   New_Line;
end Generics;
