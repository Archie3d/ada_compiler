with Ada.Text_IO; use Ada.Text_IO;
package Handler_Failure is
    Original, Replacement : exception;
end Handler_Failure;
package body Handler_Failure is
begin
    raise Original;
exception
    when Original =>
        Put_Line ("handler entered");
        raise Replacement;
    when Replacement => Put_Line ("unexpected sibling handler");
end Handler_Failure;
package Later_Package is
end Later_Package;
package body Later_Package is
begin
    Put_Line ("unexpected later package");
end Later_Package;
procedure PackageHandlerFailure is
begin
    Put_Line ("unexpected main");
end PackageHandlerFailure;
