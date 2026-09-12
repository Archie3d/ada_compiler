with Ada.Text_IO; use Ada.Text_IO;
package Startup is
    Startup_Failed : exception;
end Startup;
package body Startup is
begin
    Put_Line ("package body entered");
    raise Startup_Failed;
end Startup;
procedure ElaborationBodyFailure is
begin
    Put_Line ("main must not run");
end ElaborationBodyFailure;
