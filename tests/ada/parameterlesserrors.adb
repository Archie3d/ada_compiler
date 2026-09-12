procedure ParameterlessErrors is
    function Ambiguous return Integer is
    begin
        return 1;
    end Ambiguous;
    function Ambiguous (N : Integer := 2) return Integer is
    begin
        return N;
    end Ambiguous;
    package P is
        function Required (N : Integer) return Integer;
    end P;
    package body P is
        function Required (N : Integer) return Integer is
        begin
            return N;
        end Required;
    end P;
    function Only_Function return Integer is
    begin
        return 1;
    end Only_Function;
    X : Integer;
begin
    X := Ambiguous;
    X := P.Required;
    Only_Function;
    declare
        type A is (Shared, A_Only);
        type B is (Shared, B_Only);
    begin
        if Shared = Shared then
            null;
        end if;
    end;
end ParameterlessErrors;
