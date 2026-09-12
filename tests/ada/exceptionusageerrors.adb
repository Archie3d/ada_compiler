procedure ExceptionUsageErrors is
    function Bad return Integer is
    begin
        return;
    end Bad;
    procedure Also_Bad is
    begin
        return 1;
    end Also_Bad;
begin
    raise;
exception
    when others =>
        declare
            procedure Enclosed is
            begin
                raise;
            end Enclosed;
        begin
            null;
        end;
end ExceptionUsageErrors;

package Return_Outside is
end Return_Outside;
package body Return_Outside is
begin
    return;
end Return_Outside;
