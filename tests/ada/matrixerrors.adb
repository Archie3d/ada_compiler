procedure MatrixErrors is
    type Matrix is array (1 .. 2, 1 .. 3) of Integer;
    M : Matrix;
    N : Integer := 2;
    type Dynamic is array (1 .. N, 1 .. 3) of Integer;
    type Open_Matrix is array (Integer range <>, Integer range <>) of Integer;
    type Huge is array (1 .. 2147483647, 1 .. 2147483647) of Long_Integer;
begin
    M (1) := 0;
    N := M'Length (3);
    N := M'First (N);
    N := M (True, 1);
    N := M'Length (True);
end MatrixErrors;
