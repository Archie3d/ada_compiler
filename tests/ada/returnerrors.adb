procedure ReturnErrors is
    type Vector is array (Integer range <>) of Integer;
    function Make return Vector is
    begin
        return (1, 2, 3);
    end Make;
begin
    null;
end ReturnErrors;
