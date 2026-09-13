with Ada.Text_IO; use Ada.Text_IO;

procedure integerconversions is
    Wide : Long_Integer := 4294967296;
    Negative : Integer := -123;
    Real_Value : Long_Float := 2147483648.0;
    Small : Integer;
begin
    Put_Line (Long_Integer'Image (Long_Integer (Negative)));
    begin
        Small := Integer (Wide);
        Put_Line ("MISSED narrowing");
    exception
        when Constraint_Error => Put_Line ("caught narrowing");
    end;
    begin
        Small := Integer (Real_Value);
        Put_Line ("MISSED real conversion");
    exception
        when Constraint_Error => Put_Line ("caught real conversion");
    end;
    Real_Value := 4294967296.0;
    Put_Line (Long_Integer'Image (Long_Integer (Real_Value)));
    Real_Value := 1.0E30;
    begin
        Wide := Long_Integer (Real_Value);
        Put_Line ("MISSED huge real conversion");
    exception
        when Constraint_Error => Put_Line ("caught huge real conversion");
    end;
end integerconversions;
