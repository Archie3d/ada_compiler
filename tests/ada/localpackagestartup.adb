with Ada.Text_IO; use Ada.Text_IO;
generic
package Counter_Template is
    Value : Integer := 0;
    procedure Increment;
end Counter_Template;
package body Counter_Template is
    procedure Increment is
    begin
        Value := Value + 1;
    end Increment;
begin
    Value := 4;
end Counter_Template;
package Startup_Check is
    procedure Run;
end Startup_Check;
package body Startup_Check is
    procedure Run is
        package Counter is new Counter_Template;
    begin
        if Counter.Value /= 4 then
            raise Program_Error;
        end if;
        Counter.Increment;
        if Counter.Value /= 5 then
            raise Program_Error;
        end if;
        Put_Line ("local instance initialized at call");
    end Run;
begin
    Run;
end Startup_Check;
procedure LocalPackageStartup is
begin
    Startup_Check.Run;
end LocalPackageStartup;
