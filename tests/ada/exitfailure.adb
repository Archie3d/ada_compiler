with Ada.Text_IO; use Ada.Text_IO;
procedure ExitFailure is
begin
    Put_Line ("expected output");
    raise Program_Error;
end ExitFailure;
