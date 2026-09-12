procedure OverloadErrors is
    procedure Pick (X : Integer) is
    begin
        null;
    end Pick;
    procedure Pick (X : Long_Integer) is
    begin
        null;
    end Pick;
    procedure Pair (X : Integer; Y : Integer := 0) is
    begin
        null;
    end Pair;
begin
    Pick (1);
    Pair (X => 1, X => 2);
    Pair (X => 1, 2);
    Pair (1, X => 2);
end OverloadErrors;
