with Ada.Text_IO; use Ada.Text_IO;
procedure Reraise is
    Original, Nested : exception;
    procedure Handle_Other is
    begin
        raise Nested;
    exception
        when Nested => null;
    end Handle_Other;
    procedure Again is
    begin
        raise Original;
    exception
        when others =>
            Handle_Other;
            begin
                raise Nested;
            exception
                when Nested =>
                    begin
                        raise;
                    exception
                        when Nested => Put_Line ("inner occurrence restored");
                    end;
            end;
            raise;
    end Again;
    function Fail return Integer is
    begin
        raise Original;
        return 0;
    end Fail;
    procedure Declaration_Failure is
        I : Integer := Fail;
    begin
        Put_Line ("unexpected body");
    exception
        when Original => Put_Line ("unexpected local handler");
    end Declaration_Failure;
begin
    for I in 1 .. 2 loop
        begin
            Again;
            raise Program_Error;
        exception
            when Original => Put_Line ("outer occurrence restored");
        end;
    end loop;
    begin
        Declaration_Failure;
        raise Program_Error;
    exception
        when Original => Put_Line ("declaration failure reaches caller");
    end;
    begin
        begin
            raise Original;
        exception
            when Original => raise Nested;
            when Nested => Put_Line ("unexpected sibling handler");
        end;
    exception
        when Nested => Put_Line ("handler failure reaches outer block");
    end;
end Reraise;
