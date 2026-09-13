with Ada.Text_IO;
with Ada.Numerics.Long_Elementary_Functions;
with Numerics_Checks;
procedure Numerics64 is
    type Custom is digits 7 range 1.0 .. 2.0;
    subtype Narrow is Custom range 1.0 .. 1.5;
    package Predefined_Checks is new Numerics_Checks (Long_Float);
    package Custom_Checks is new Numerics_Checks (Narrow);
    Result : Long_Float;
begin
    Predefined_Checks.Run;
    Custom_Checks.Run;
    Result := Ada.Numerics.Long_Elementary_Functions.Sqrt (9.0);
    if Result /= 3.0 then
        raise Program_Error;
    end if;
    Ada.Text_IO.Put_Line ("numerics64 ok");
end Numerics64;
