-- Slice assignment must preserve the source value when storage overlaps.
with Ada.Text_IO; use Ada.Text_IO;
procedure SliceAssignment is
    S : String (1 .. 6) := "abcdef";
    Low : Integer := 2;
    High : Integer := 6;
    procedure Check (Condition : Boolean; Message : String) is
    begin
        if not Condition then
            Put_Line ("FAILED: " & Message & "; got " & S);
            raise Program_Error;
        end if;
    end Check;
begin
    S (2 .. 6) := S (1 .. 5);
    Check (S = "aabcde", "right overlap");
    S := "abcdef";
    S (1 .. 5) := S (2 .. 6);
    Check (S = "bcdeff", "left overlap");
    S := "abcdef";
    S (Low .. High) := S (Low - 1 .. High - 1);
    Check (S = "aabcde", "runtime overlap");
    S := S;
    Check (S = "aabcde", "self assignment");
    begin
        S (Low .. High) := "bad";
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Check (S = "aabcde", "failed assignment preserves target");
    Put_Line ("sliceassignment: passed");
end SliceAssignment;
