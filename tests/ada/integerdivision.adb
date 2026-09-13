with Ada.Text_IO; use Ada.Text_IO;

procedure integerdivision is
    X : Long_Integer := Long_Integer'First;
    Zero : Long_Integer := 0;
    Minus_One : Long_Integer := -1;
    Result : Long_Integer;
begin
    begin
        Result := X / Zero;
        Put_Line ("MISSED /");
    exception
        when Constraint_Error => Put_Line ("caught /");
    end;
    begin
        Result := X rem Zero;
        Put_Line ("MISSED rem");
    exception
        when Constraint_Error => Put_Line ("caught rem");
    end;
    begin
        Result := X mod Zero;
        Put_Line ("MISSED mod");
    exception
        when Constraint_Error => Put_Line ("caught mod");
    end;
    Put_Line (Long_Integer'Image (X rem Minus_One));
    Put_Line (Long_Integer'Image (X mod Minus_One));
end integerdivision;
