procedure DefaultErrors is
    procedure Bad_Mode (X : out Integer := 1) is
    begin
        null;
    end Bad_Mode;
    procedure Bad_Type (X : Boolean := 1) is
    begin
        null;
    end Bad_Type;
    procedure Bad_Real (X : Integer := 1.0) is
    begin
        null;
    end Bad_Real;
begin
    null;
end DefaultErrors;
