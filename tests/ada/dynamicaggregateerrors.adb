procedure DynamicAggregateErrors is
    Size : Integer := 3;
    type Vector is array (Integer range <>) of Integer;
    Mixed : Vector (1 .. Size) := (1, 2 => 3);
    Duplicate : Vector (1 .. Size) := (1 .. 2 => 0, 2 .. 3 => 1);
    Gap : Vector (1 .. Size) := (1 => 0, 3 => 1);
    Dynamic : Vector (1 .. Size) := (Size => 0, others => 1);
begin
    null;
end DynamicAggregateErrors;
