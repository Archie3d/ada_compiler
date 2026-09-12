with Ada.Text_IO; use Ada.Text_IO;
package Initialization is
    function Fail return Integer;
end Initialization;
package body Initialization is
    function Fail return Integer is
    begin
        Put_Line ("initializer entered");
        raise Constraint_Error;
        return 0;
    end Fail;
    Value : Integer := Fail;
end Initialization;
procedure ElaborationFailure is
begin
    Put_Line ("main must not run");
exception
    when others => Put_Line ("main must not handle elaboration failure");
end ElaborationFailure;
