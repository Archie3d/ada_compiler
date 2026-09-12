with Ada.Text_IO; use Ada.Text_IO;
procedure FunctionFallthrough is
    function Integer_Result (Return_Now : Boolean) return Integer is
    begin
        if Return_Now then
            return 7;
        end if;
    end Integer_Result;
    function Real_Result (Return_Now : Boolean) return Float is
    begin
        if Return_Now then
            return 1.0;
        end if;
    end Real_Result;
    function Double_Result return Long_Float is
    begin
        null;
    end Double_Result;
    type Integer_Access is access Integer;
    function Access_Result return Integer_Access is
    begin
        null;
    end Access_Result;
    function Handled_Result return Long_Integer is
    begin
        raise Constraint_Error;
    exception
        when Constraint_Error => null;
    end Handled_Result;
    I : Integer;
    F : Float;
    L : Long_Integer;
    D : Long_Float;
    A : Integer_Access;
begin
    I := Integer_Result (True);
    if I /= 7 then
        raise Constraint_Error;
    end if;
    begin
        I := Integer_Result (False);
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("integer fallthrough");
    end;
    begin
        F := Real_Result (False);
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("real fallthrough");
    end;
    begin
        D := Double_Result;
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("double fallthrough");
    end;
    begin
        A := Access_Result;
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("access fallthrough");
    end;
    begin
        L := Handled_Result;
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("handler fallthrough");
    end;
end FunctionFallthrough;
