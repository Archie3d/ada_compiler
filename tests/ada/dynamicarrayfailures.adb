with Ada.Text_IO; use Ada.Text_IO;
procedure DynamicArrayFailures is
    type Vector is array (Integer range <>) of Integer;
    procedure Too_Large (Low, High : Integer) is
        Value : Vector (Low .. High);
    begin
        raise Program_Error;
    end Too_Large;
    procedure Bad_Init (Size : Integer) is
        Value : String (1 .. Size) := "abc";
    begin
        raise Program_Error;
    end Bad_Init;
    procedure Bounds (Low : Integer) is
        Value : String (Low .. 2);
    begin
        raise Program_Error;
    end Bounds;
    function Early (Size : Integer; Fail : Boolean) return Integer is
        Value : Vector (1 .. Size) := (others => 5);
    begin
        if Fail then
            raise Constraint_Error;
        end if;
        return Value (1);
    end Early;
    Result : Integer;
begin
    begin
        Too_Large (Integer'First, Integer'Last);
    exception
        when Storage_Error => Put_Line ("oversized length rejected");
    end;
    begin
        Bad_Init (4);
    exception
        when Constraint_Error => Put_Line ("initializer length checked");
    end;
    begin
        Bounds (0);
    exception
        when Constraint_Error => Put_Line ("index subtype checked");
    end;
    for I in 1 .. 50 loop
        Result := Early (1000, False);
        if Result /= 5 then
            raise Program_Error;
        end if;
        begin
            Result := Early (1000, True);
            raise Program_Error;
        exception
            when Constraint_Error => null;
        end;
    end loop;
    Put_Line ("normal and exceptional array lifetimes");
end DynamicArrayFailures;
