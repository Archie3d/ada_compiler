-- A pile of integers whose representation stays inside this package.  Users
-- get the name, the operations and a constant for the empty one; how it is
-- laid out is written below the word 'private' and is theirs to ignore.

package Piles is

   Depth : constant := 8;

   type Pile is private;

   -- Named here without a value, which the private part supplies once the
   -- representation is known.
   Empty : constant Pile;

   Overflow  : exception;
   Underflow : exception;

   procedure Push (P : in out Pile; Value : in Integer);
   procedure Pop (P : in out Pile; Value : out Integer);

   function Size (P : in Pile) return Integer;
   function Is_Empty (P : in Pile) return Boolean;

private

   type Contents is array (1 .. Depth) of Integer;

   type Pile is
      record
         Items : Contents;
         Top   : Integer := 0;
      end record;

   Empty : constant Pile := (Items => (others => 0), Top => 0);

end Piles;
