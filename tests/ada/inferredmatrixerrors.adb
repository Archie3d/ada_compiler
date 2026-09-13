procedure InferredMatrixErrors is
    type Matrix is array (Positive range <>, Positive range <>) of Integer;
    A : Matrix := (others => (1 .. 2 => 0));
    B : Matrix := (1 .. 2 => (others => 0));
begin
    null;
end InferredMatrixErrors;
