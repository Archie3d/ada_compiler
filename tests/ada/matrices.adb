with Ada.Text_IO; use Ada.Text_IO;
procedure Matrices is
    type Matrix is array (2 .. 3, 5 .. 7) of Integer;
    A : Matrix := ((1, 2, 3), (4, 5, 6));
    B : Matrix := (2 => (5 => 1, 6 .. 7 => 2), 3 => (others => 3));
    function Copy (Value : Matrix) return Matrix is
    begin
        return Value;
    end Copy;
    procedure Change (Value : in out Matrix) is
        procedure Inner is
        begin
            Value (2, 5) := 9;
        end Inner;
    begin
        Inner;
    end Change;
    Total : Integer := 0;
begin
    if A'First /= 2 or A'Last (2) /= 7 or Matrix'Length (1) /= 2 or A'Length (2) /= 3 then
        raise Program_Error;
    end if;
    for I in A'Range (1) loop
        for J in A'Range (2) loop
            Total := Total + A (I, J);
        end loop;
    end loop;
    if Total /= 21 then
        raise Program_Error;
    end if;
    if B (2, 5) /= 1 or B (2, 7) /= 2 or B (3, 6) /= 3 then
        raise Program_Error;
    end if;
    B := Copy (A);
    if B /= A then
        raise Program_Error;
    end if;
    Change (B);
    if B (2, 5) /= 9 or B = A then
        raise Program_Error;
    end if;
    if Copy (A) (3, 7) /= 6 then
        raise Program_Error;
    end if;
    Put_Line ("matrix aggregates, attributes, range loops, calls, returns, and equality");
    begin
        B (1, 5) := 0;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("first dimension checked");
    end;
    begin
        B (2, 8) := 0;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("second dimension checked");
    end;
end Matrices;
