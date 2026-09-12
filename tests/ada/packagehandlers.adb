with Ada.Text_IO; use Ada.Text_IO;
package Recovery is
    Original : exception;
    Value : Integer := 0;
end Recovery;
package body Recovery is
    package Nested is
    end Nested;
    package body Nested is
    begin
        raise Original;
    exception
        when Original =>
            begin
                raise Constraint_Error;
            exception
                when Constraint_Error => null;
            end;
            begin
                raise;
            exception
                when Original => Put_Line ("nested package re-raise");
            end;
    end Nested;
begin
    raise Original;
exception
    when Original =>
        Value := 7;
        Put_Line ("package recovered");
end Recovery;
package Following is
    Seen : Integer := Recovery.Value;
end Following;
procedure PackageHandlers is
begin
    if Following.Seen /= 7 then
        raise Program_Error;
    end if;
    Put_Line ("main after recovery");
end PackageHandlers;
