procedure ClosingNameErrors is
    package Inner is
    end Wrong_Spec;
    package body Inner is
    end Wrong_Body;
    procedure Nested is
    begin
        null;
    end Wrong_Procedure;
begin
    Named_Loop : loop
        exit;
    end loop Wrong_Loop;
    Named_Block : declare
        Value : Integer := 1;
    begin
        null;
    end Wrong_Block;
end Other_Name;
