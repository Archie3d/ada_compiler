-- Value accepts whitespace and case variations, and rejects malformed text.
with Ada.Text_IO; use Ada.Text_IO;
procedure ValueParsing is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    type Colour is (Red, Green, Blue);
    C : Colour;
    procedure Bad_Integer (Text : String) is
        Value : Integer;
    begin
        Value := Integer'Value (Text);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end Bad_Integer;
begin
    Check (Integer'Value ("  +42  ") = 42);
    Check (Integer'Value ("-12") = -12);
    Check (Colour'Value (" gReEn ") = Green);
    Check (Boolean'Value ("true") = True);
    Bad_Integer ("");
    Bad_Integer ("   ");
    Bad_Integer ("12junk");
    Bad_Integer ("++1");
    Bad_Integer ("2147483648");
    begin
        C := Colour'Value ("purple");
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Put_Line ("valueparsing: passed");
end ValueParsing;
