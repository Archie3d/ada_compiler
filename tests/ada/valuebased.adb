-- Integer Value supports based literals, separators, exponents and range checks.
with Ada.Text_IO; use Ada.Text_IO;
procedure ValueBased is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    procedure Bad_Value (Text : String) is
        Value : Long_Integer;
    begin
        Value := Long_Integer'Value (Text);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end Bad_Value;
begin
    Check (Integer'Value ("16#FF#") = 255);
    Check (Integer'Value (" -16#f_f# ") = -255);
    Check (Integer'Value ("2#101#E+3") = 40);
    Check (Integer'Value ("1_6#A#e1") = 160);
    Check (Integer'Value ("1_000e+2") = 100_000);
    Check (Long_Integer'Value ("16#7FFF_FFFF_FFFF_FFFF#") = Long_Integer'Last);
    Check (Long_Integer'Value ("-16#8000_0000_0000_0000#") = Long_Integer'First);
    Check (Integer'Value ("0e999999999999999999999") = 0);
    Bad_Value ("1#1#");
    Bad_Value ("17#1#");
    Bad_Value ("2#2#");
    Bad_Value ("16##");
    Bad_Value ("16#FF");
    Bad_Value ("16#FF#junk");
    Bad_Value ("16#F.F#");
    Bad_Value ("_1");
    Bad_Value ("1_");
    Bad_Value ("1__0");
    Bad_Value ("16#_F#");
    Bad_Value ("16#F_#");
    Bad_Value ("1e");
    Bad_Value ("1e+");
    Bad_Value ("1e-1");
    Bad_Value ("1e_1");
    Bad_Value ("1e1_");
    Bad_Value ("1e1__0");
    Bad_Value ("16#8000_0000_0000_0000#");
    Bad_Value ("-16#8000_0000_0000_0001#");
    Bad_Value ("1e999999999999999999999");
    Put_Line ("valuebased: passed");
end ValueBased;
