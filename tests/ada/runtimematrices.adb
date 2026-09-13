with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeMatrices is
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    subtype Fixed is Matrix (2 .. 3, 5 .. 7);
    procedure Run (Rows, Columns : Integer) is
        A : Matrix (2 .. Rows + 1, 5 .. Columns + 4) := (others => (others => 1));
        B : Matrix (-2 .. Rows - 3, 8 .. Columns + 7);
        function Copy (Value : Matrix) return Matrix is
            Local : Matrix := Value;
        begin
            return Local;
        end Copy;
        procedure Change (Value : in out Matrix) is
            procedure Inner is
            begin
                Value (Value'First (1), Value'Last (2)) := 9;
            end Inner;
        begin
            Inner;
        end Change;
        procedure Change_Local is
        begin
            A (A'Last (1), A'First (2)) := 4;
        end Change_Local;
        Total : Integer := 0;
    begin
        if A'Length (1) /= Rows or A'Length (2) /= Columns then
            raise Program_Error;
        end if;
        B := Copy (A);
        if A /= B then
            raise Program_Error;
        end if;
        Change (B);
        Change_Local;
        if B (-2, Columns + 7) /= 9 or A (Rows + 1, 5) /= 4 then
            raise Program_Error;
        end if;
        for I in B'Range (1) loop
            for J in B'Range (2) loop
                Total := Total + B (I, J);
            end loop;
        end loop;
        if Total /= Rows * Columns + 8 or A = B then
            raise Program_Error;
        end if;
        declare
            C : Matrix := Copy (B);
        begin
            if C'First (1) /= -2 or C'First (2) /= 8 or C /= B then
                raise Program_Error;
            end if;
        end;
        begin
            B (-2, Columns + 8) := 0;
            raise Program_Error;
        exception
            when Constraint_Error => null;
        end;
    end Run;
    F : Fixed := ((1, 2, 3), (4, 5, 6));
    procedure Check (Value : Matrix) is
    begin
        if Value'First (1) /= 2 or Value'Last (2) /= 7 or Value (3, 7) /= 6 then
            raise Program_Error;
        end if;
    end Check;
begin
    Check (F);
    Run (2, 3);
    Run (3, 2);
    Put_Line ("runtime matrices: bounds, sliding, captures, parameters, and returns");
end RuntimeMatrices;
