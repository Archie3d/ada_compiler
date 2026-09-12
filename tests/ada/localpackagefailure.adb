with Ada.Text_IO; use Ada.Text_IO;
procedure LocalPackageFailure is
    Attempts : Integer := 0;
    function Fail return Integer is
    begin
        Attempts := Attempts + 1;
        raise Constraint_Error;
        return 0;
    end Fail;
    procedure Run is
        generic
        package Template is
        end Template;
        package body Template is
            Value : Integer := Fail;
        begin
            Put_Line ("unexpected package body");
        exception
            when Constraint_Error => Put_Line ("unexpected package handler");
        end Template;
        package Instance is new Template;
    begin
        Put_Line ("unexpected enclosing body");
    exception
        when Constraint_Error => Put_Line ("unexpected enclosing handler");
    end Run;
begin
    if Attempts /= 0 then
        raise Program_Error;
    end if;
    for I in 1 .. 2 loop
        begin
            Run;
            raise Program_Error;
        exception
            when Constraint_Error => Put_Line ("local declaration failure reaches caller");
        end;
    end loop;
    if Attempts /= 2 then
        raise Program_Error;
    end if;
    begin
        declare
            package Local is
            end Local;
            package body Local is
            begin
                raise Constraint_Error;
            exception
                when Constraint_Error => raise;
            end Local;
        begin
            Put_Line ("unexpected block body");
        end;
    exception
        when Constraint_Error => Put_Line ("local package re-raise reaches outer handler");
    end;
end LocalPackageFailure;
