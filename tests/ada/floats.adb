with Ada.Text_IO;
with Ada.Text_IO.Float_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Floats is

    type Coefficient is digits 10 range -1.0 .. 1.0;
    type Real is digits 8;
    type Mass is digits 7 range 0.0 .. 1.0E35;
    subtype Probability is Real range 0.0 .. 1.0;

    type Narrow is digits 6 range -10.0 .. 10.0;

    --  Each floating point type reads and writes through its own instance, and
    --  the defaults Ada derives from the type come with it: a Coefficient shows
    --  nine digits after the point where a Narrow shows five.
    package Coefficient_IO is new Ada.Text_IO.Float_IO (Coefficient);
    package Real_IO is new Ada.Text_IO.Float_IO (Real);
    package Mass_IO is new Ada.Text_IO.Float_IO (Mass);
    package Narrow_IO is new Ada.Text_IO.Float_IO (Narrow);

    use Coefficient_IO, Real_IO, Mass_IO, Narrow_IO;

    Pi : constant := 3.14159_26535_89793;

    type Point is record
        Near : Narrow;
        Far  : Real;
    end record;

    type Samples is array (1 .. 3) of Real;

    C : Coefficient := 0.5;
    R : Real := 2.5;
    M : Mass := 1.0E30;
    P : Probability := 0.25;
    N : Narrow := 1.5;

    Origin : Point := (Near => 1.25, Far => 2.5);
    Data   : Samples := (1.0, 2.5, 4.0);

    function Scaled (Value : Real; By : Real) return Real is
    begin
        return Value * By;
    end Scaled;

    procedure Bump (Value : in out Real; By : Real) is
    begin
        Value := Value + By;
    end Bump;

begin
    Put ("default ");
    Put (C);
    Put (R);
    Put (M);
    New_Line;

    Put ("fixed   ");
    Put (R, 3, 2, 0);
    Put (P, 3, 2, 0);
    Put (N, 3, 2, 0);
    New_Line;

    Put ("digits  ");
    Put (Coefficient'Digits, 3);
    Put (Real'Digits, 3);
    Put (Mass'Digits, 3);
    Put (Narrow'Digits, 3);
    New_Line;

    Put ("bounds  ");
    Put (Narrow'First, 4, 2, 0);
    Put (Narrow'Last, 4, 2, 0);
    Put (Mass'First, 4, 2, 0);
    New_Line;

    Put_Line ("image  " & Real'Image (R) & Narrow'Image (N));

    Put ("arith   ");
    Put (C * 0.5 - 0.25, 3, 4, 0);
    Put (R / 2.0, 3, 4, 0);
    Put (abs (-N), 3, 4, 0);
    Put (N ** 3, 3, 4, 0);
    New_Line;

    Put ("named   ");
    Put (Real (Pi), 3, 5, 0);
    Put (Narrow (Pi), 3, 5, 0);
    New_Line;

    Put ("convert ");
    Put (Integer (R * 2.0), 4);
    Put (Integer (2.5), 4);
    Put (Integer (-2.5), 4);
    Put (Real (7) / 2.0, 4, 2, 0);
    New_Line;

    Put ("compose ");
    Put (Origin.Near, 3, 2, 0);
    Put (Origin.Far, 3, 2, 0);
    Put (Real (Origin.Near) + Origin.Far, 3, 2, 0);
    New_Line;

    declare
        Total : Real := 0.0;
    begin
        for I in 1 .. 3 loop
            Total := Total + Data (I);
        end loop;
        Bump (Total, 0.5);
        Put ("total   ");
        Put (Total, 3, 2, 0);
        Put (Scaled (Total, 2.0), 4, 2, 0);
        New_Line;
    end;

    if R > 2.0 and then P < 1.0 then
        Put_Line ("compared");
    end if;

    begin
        P := P + 1.0;
        Put_Line ("no check on the assignment");
    exception
        when Constraint_Error =>
            Put_Line ("assignment out of range");
    end;

    begin
        C := Coefficient (R);
        Put_Line ("no check on the conversion");
    exception
        when Constraint_Error =>
            Put_Line ("conversion out of range");
    end;

    begin
        N := Narrow (Scaled (R, 100.0));
        Put_Line ("no check on the result");
    exception
        when Constraint_Error =>
            Put_Line ("result out of range");
    end;
end Floats;
