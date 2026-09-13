procedure MatrixAggregateErrors is
    type Vector is array (1 .. 2) of Integer;
    type Matrix is array (Positive range <>, Positive range <>) of Integer;
    Row : Vector := (1, 2);
    Bad : Matrix := (Row, Row);
begin
    null;
end MatrixAggregateErrors;
