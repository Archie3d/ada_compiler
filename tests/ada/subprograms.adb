with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Subprograms is

    function Factorial (N : Integer) return Integer is
    begin
        if N <= 1 then
            return 1;
        end if;
        return N * Factorial (N - 1);
    end Factorial;

    procedure Swap (X, Y : in out Integer) is
        Temp : Integer;
    begin
        Temp := X;
        X := Y;
        Y := Temp;
    end Swap;

    procedure Divide (Left, Right : Integer; Quotient, Rest : out Integer) is
    begin
        Quotient := Left / Right;
        Rest := Left rem Right;
    end Divide;

    procedure Report (Label : String) is
        Scale : Integer := 10;

        function Scaled (Value : Integer) return Integer is
        begin
            return Value * Scale;
        end Scaled;

    begin
        Put (Label);
        Put (Scaled (3), 4);
        New_Line;
    end Report;

    A, B : Integer;
    Q, R : Integer;

begin
    Put ("factorial:");
    Put (Factorial (6), 5);
    New_Line;

    A := 1;
    B := 2;
    Swap (A, B);
    Put ("swap:");
    Put (A, 3);
    Put (B, 3);
    New_Line;

    Divide (17, 5, Q, R);
    Put ("divide:");
    Put (Q, 3);
    Put (R, 3);
    New_Line;

    Report ("nested:");
end Subprograms;
