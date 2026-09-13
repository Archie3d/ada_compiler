with Ada.Text_IO; use Ada.Text_IO;
procedure InferredMatrices is
    subtype Column is Integer range 5 .. 20;
    type Matrix is array (Positive range <>, Column range <>) of Integer;
    A : Matrix := ((1, 2, 3), (4, 5, 6));
    B : Matrix := (3 .. 4 => (7 .. 9 => 2));
    procedure Check (Value : Matrix) is
    begin
        if Value'First (1) /= 1 or Value'First (2) /= 5
            or Value'Length (1) /= 2 or Value'Length (2) /= 3 or Value (2, 7) /= 6 then
            raise Program_Error;
        end if;
    end Check;
    function Make (Rows, Columns : Integer) return Matrix is
    begin
        return (2 .. Rows + 1 => (6 .. Columns + 5 => 8));
    end Make;
    C : Matrix := Make (3, 2);
    type Cube is array (Positive range <>, Positive range <>, Positive range <>) of Integer;
    D : Cube := (((1, 2), (3, 4)), ((5, 6), (7, 8)));
    type Letters is array (Positive range <>, Column range <>) of Character;
    Text : Letters := ("abc", "def");
    type Item is record
        Value : Integer;
    end record;
    type Items is array (Positive range <>, Positive range <>) of Item;
    Grid : Items := ((1 => (Value => 11)), (1 => (Value => 12)));
begin
    Check (A);
    Check (((1, 2, 3), (4, 5, 6)));
    if B'First (1) /= 3 or B'First (2) /= 7 or B (4, 9) /= 2 then
        raise Program_Error;
    end if;
    if C'First (1) /= 2 or C'First (2) /= 6 or C'Length (1) /= 3
        or C'Length (2) /= 2 or C (4, 7) /= 8 then
        raise Program_Error;
    end if;
    if D'Length (3) /= 2 or D (2, 2, 2) /= 8 then
        raise Program_Error;
    end if;
    if Text'First (2) /= 5 or Text'Length (2) /= 3 or Text (2, 7) /= 'f' then
        raise Program_Error;
    end if;
    if Grid (2, 1).Value /= 12 then
        raise Program_Error;
    end if;
    A := (8 .. 9 => (10 .. 12 => 3));
    if A'First (1) /= 1 or A'First (2) /= 5 or A (2, 7) /= 3 then
        raise Program_Error;
    end if;
    Put_Line ("inferred matrices: positional, named, arguments, returns, strings, and record cells");
end InferredMatrices;
