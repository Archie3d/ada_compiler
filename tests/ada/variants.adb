-- Discriminants and variant records.  A discriminant is fixed when an object is
-- declared and selects which components the value carries; the record is laid
-- out at its largest, so every object of it is the same size.

with Ada.Text_IO;
with Ada.Integer_Text_IO;

use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Variants is

   type Figure is (Point, Circle, Rectangle, Triangle);

   type Shape (Kind : Figure) is
      record
         Label : Character;
         case Kind is
            when Point =>
               null;
            when Circle =>
               Radius : Integer;
            when others =>
               Width  : Integer;
               Height : Integer;
         end case;
      end record;

   -- A subtype may fix the discriminant, which settles the components once and
   -- for all for anything declared with it.
   subtype Round is Shape (Circle);

   -- A parameter of the unconstrained type takes a shape of any kind, and the
   -- value it is given says which components are there.
   function Area (S : in Shape) return Integer is
   begin
      case S.Kind is
         when Point =>
            return 0;
         when Circle =>
            return 3 * S.Radius * S.Radius;
         when Rectangle =>
            return S.Width * S.Height;
         when Triangle =>
            return S.Width * S.Height / 2;
      end case;
   end Area;

   function Radius_Of (S : in Shape) return Integer is
   begin
      return S.Radius;
   end Radius_Of;

   C : Round := (Kind => Circle, Label => 'c', Radius => 5);
   R : Shape (Rectangle) := (Rectangle, 'r', 3, 4);
   T : Shape (Triangle) := (Triangle, 't', 6, 5);
   P : Shape (Point);

begin
   P.Label := 'p';

   Put (C.Label); Put (Area (C), 5); New_Line;
   Put (R.Label); Put (Area (R), 5); New_Line;
   Put (T.Label); Put (Area (T), 5); New_Line;
   Put (P.Label); Put (Area (P), 5); New_Line;

   -- Every alternative shares the storage of the largest, so one size fits all.
   Put ("size"); Put (Shape'Size / 8, 4); New_Line;
   Put ("round"); Put (Round'Size / 8, 3); New_Line;

   -- The discriminant is a component like any other to read.
   Put_Line (Figure'Image (C.Kind) & " " & Figure'Image (P.Kind));

   -- Where nothing fixed the discriminant, reaching for a component of the
   -- wrong alternative is found out when the value arrives.
   begin
      Put (Radius_Of (R), 3);
      Put_Line ("unreachable");
   exception
      when Constraint_Error =>
         Put_Line ("a rectangle has no radius");
   end;

   -- An 'others' alternative covers what the named ones left out, so a triangle
   -- carries the same components a rectangle does.
   Put ("triangle"); Put (T.Width, 3); Put (T.Height, 3); New_Line;

end Variants;
