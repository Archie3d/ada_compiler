with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeMatrixChecks is
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    type Cube is array (Integer range <>, Integer range <>, Integer range <>) of Integer;
    N : Integer := 2;
    Calls : Integer := 0;
    function Next return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next;
    A : Matrix (1 .. N, 3 .. 5) := (others => (others => 7));
    Wrong : Matrix (1 .. N, 3 .. 4) := (others => (others => 0));
    Empty : Matrix (1 .. N - 2, 1 .. 3) := (others => (others => Next));
    Other_Empty : Matrix (1 .. N - 2, 1 .. 2);
    subtype Fixed is Matrix (8 .. 9, -2 .. 0);
    F : Fixed;
    procedure Fixed_Argument (Value : Fixed) is
    begin
        if Value (8, -2) /= 7 then
            raise Program_Error;
        end if;
    end Fixed_Argument;
    function Fixed_Result (Value : Matrix) return Fixed is
    begin
        return Value;
    end Fixed_Result;
    C : Cube (2 .. N + 1, -1 .. N - 2, 5 .. N + 4) := (others => (others => (others => 3)));
    function Copy (Value : Cube) return Cube is
    begin
        return Value;
    end Copy;
    D : Cube := Copy (C);
    type Item is record
        Value : Integer := 11;
    end record;
    type Items is array (Integer range <>, Integer range <>) of Item;
    Grid : Items (1 .. N, 1 .. N);
begin
    if Calls /= 0 or Empty = Other_Empty or Empty'Length (2) /= 3 then
        raise Program_Error;
    end if;
    if C /= D or D'First (2) /= -1 or D'Length (3) /= N or D (3, 0, 6) /= 3 then
        raise Program_Error;
    end if;
    if Grid (N, N).Value /= 11 then
        raise Program_Error;
    end if;
    F := A;
    Fixed_Argument (A);
    F := Fixed_Result (A);
    if F (9, 0) /= 7 then
        raise Program_Error;
    end if;
    begin
        A := Wrong;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("inner dimension assignment checked");
    end;
    begin
        Fixed_Argument (Wrong);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("constrained parameter shape checked");
    end;
    begin
        F := Fixed_Result (Wrong);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("constrained result shape checked");
    end;
    begin
        A := ((1, 2, 3), (4, 5));
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("runtime aggregate shape checked");
    end;
    if A (1, 3) /= 7 or A (2, 5) /= 7 then
        raise Program_Error;
    end if;
    begin
        A (N + 1, 3) := 0;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("outer dimension index checked");
    end;
    begin
        declare
            Huge : Cube (1 .. N * 1000000000, 1 .. N * 1000000000, 1 .. N * 1000000000);
        begin
            raise Program_Error;
        end;
    exception
        when Storage_Error => Put_Line ("runtime storage size overflow checked");
    end;
    Put_Line ("null shapes, three dimensions, fixed subtypes, and component defaults");
end RuntimeMatrixChecks;
