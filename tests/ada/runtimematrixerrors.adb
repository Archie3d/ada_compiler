procedure RuntimeMatrixErrors is
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    N : Integer := 2;
    Missing : Matrix;
    Few : Matrix (1 .. 2);
    Many : Matrix (1 .. 2, 1 .. 2, 1 .. 2);
    subtype Dynamic is Matrix (1 .. N, 1 .. N);
    type Wrong_Cell is record
        Value : Matrix;
    end record;
    subtype Fixed is Matrix (1 .. 2, 1 .. 2);
    Again : Fixed (1 .. 2, 1 .. 2);
begin
    N := Matrix'Length (2);
end RuntimeMatrixErrors;
