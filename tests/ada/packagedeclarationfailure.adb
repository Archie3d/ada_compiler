with Ada.Text_IO; use Ada.Text_IO;
package Declaration_Failure is
    function Fail return Integer;
end Declaration_Failure;
package body Declaration_Failure is
    function Fail return Integer is
    begin
        Put_Line ("declaration entered");
        raise Constraint_Error;
        return 0;
    end Fail;
    Value : Integer := Fail;
begin
    Put_Line ("unexpected package body");
exception
    when Constraint_Error => Put_Line ("unexpected package handler");
end Declaration_Failure;
procedure PackageDeclarationFailure is
begin
    Put_Line ("unexpected main");
end PackageDeclarationFailure;
