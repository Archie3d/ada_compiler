with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Arithmetic is

    Two    : constant Integer := 2;
    Answer : constant := 42;

    A     : Integer := 7;
    B     : Integer := 3;
    Value : Integer;
    Flag  : Boolean;

begin
    Value := A + B * Two;
    Put ("sum:");
    Put (Value, 4);
    New_Line;

    Value := (A - B) * 4 / 2;
    Put ("mul:");
    Put (Value, 4);
    New_Line;

    Put ("rem:");
    Put (A rem B, 4);
    Put (" mod:");
    Put ((-A) mod B, 4);
    New_Line;

    Put ("pow:");
    Put (Two ** 5, 4);
    Put (" abs:");
    Put (abs (B - A), 4);
    New_Line;

    Put ("num:");
    Put (Answer, 4);
    New_Line;

    Flag := A > B and then B /= 0;
    if Flag then
        Put_Line ("a is greater");
    else
        Put_Line ("a is not greater");
    end if;

    if not (A = B) or else False then
        Put_Line ("a and b differ");
    end if;
end Arithmetic;
