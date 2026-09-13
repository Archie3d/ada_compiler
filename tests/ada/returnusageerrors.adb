procedure ReturnUsageErrors is
    procedure Bad_Procedure is
    begin
        return 1;
    end Bad_Procedure;
    function Missing_Value return Integer is
    begin
        return;
    end Missing_Value;
    function Wrong_Type return Integer is
    begin
        return True;
    end Wrong_Type;
begin
    null;
end ReturnUsageErrors;
