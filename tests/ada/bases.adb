with Ada.Text_IO;
with Ada.Text_IO.Float_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Bases is

    Hex     : constant Integer := 16#FF#;
    Binary  : constant Integer := 2#1010_1010#;
    Octal   : constant Integer := 8#777#;
    Twelve  : constant Integer := 12#BB#;
    Scaled  : constant Integer := 16#1#E4;
    Grouped : constant Integer := 16#DEAD_BEE#;

    type Real is digits 10;
    package Real_IO is new Ada.Text_IO.Float_IO (Real);
    use Real_IO;

    Half    : constant Real := 16#F.8#;
    Eighth  : constant Real := 2#1.101#E3;
    Tiny    : constant Real := 16#1.0#E-2;
    Decimal : constant Real := 10#2.5#E1;

begin
    Put_Line ("Integer literals");
    Put (Hex, 6);
    Put (Binary, 6);
    Put (Octal, 6);
    Put (Twelve, 6);
    New_Line;
    Put (Scaled, 10);
    Put (Grouped, 12);
    New_Line;

    Put_Line ("Real literals");
    Put (Half, 3, 3, 0);
    New_Line;
    Put (Eighth, 3, 3, 0);
    New_Line;
    Put (Tiny, 3, 6, 0);
    New_Line;
    Put (Decimal, 3, 3, 0);
    New_Line;

    Put_Line ("Based output");
    Put (255, 0, 16);
    New_Line;
    Put (170, 0, 2);
    New_Line;
    Put (511, 0, 8);
    New_Line;
    Put (-255, 0, 16);
    New_Line;
    Put (255, 12, 16);
    New_Line;
    Put (255, 0, 10);
    New_Line;
end Bases;
