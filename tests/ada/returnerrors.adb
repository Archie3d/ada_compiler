procedure ReturnErrors is
    type Vector is array (Integer range <>) of Integer;
    function Make return Vector is
    begin
        return (others => 0);
    end Make;
begin
    null;
end ReturnErrors;
