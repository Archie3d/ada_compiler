procedure LimitedPrivateErrors is
    package Hidden is
        type Item is limited private;
    private
        type Item is record
            Value : Integer;
        end record;
    end Hidden;
    A, B : Hidden.Item;
begin
    A := B;
    if A = B then
        null;
    end if;
end LimitedPrivateErrors;
