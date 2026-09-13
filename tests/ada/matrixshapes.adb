with Ada.Text_IO; use Ada.Text_IO;
procedure MatrixShapes is
    type Side is (Left, Right);
    type Cube is array (Side, -1 .. 0, 4 .. 5) of Integer;
    C : Cube := (((1, 2), (3, 4)), ((5, 6), (7, 8)));
    Calls : Integer := 0;
    function Next return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next;
    type Empty is array (1 .. 2, 4 .. 3) of Integer;
    A : Empty := (others => (others => Next));
    B : Empty;
    type Item is record
        Value : Integer;
    end record;
    type Items is array (1 .. 2, 1 .. 2) of Item;
    Grid : Items := (others => (others => (Value => 11)));
    type Matrix is array (1 .. 2, 1 .. 3) of Integer;
    M : Matrix := (others => (others => 7));
begin
    if C (Right, 0, 5) /= 8 or C'First (1) /= Left or C'Length (3) /= 2 then
        raise Program_Error;
    end if;
    if A'Length (1) /= 2 or A'Length (2) /= 0 or Calls /= 0 then
        raise Program_Error;
    end if;
    B := A;
    if A /= B or Grid (2, 2).Value /= 11 then
        raise Program_Error;
    end if;
    Put_Line ("three dimensions, enum indices, record cells, and null dimensions");
    begin
        A (1, 4) := 9;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("null dimension rejects indexing");
    end;
    begin
        M := ((1, 2, 3), (4, 5));
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("aggregate row shape checked");
    end;
    if M (1, 1) /= 7 or M (2, 3) /= 7 then
        raise Program_Error;
    end if;
end MatrixShapes;
