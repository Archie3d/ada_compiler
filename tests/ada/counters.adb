with Ada.Text_IO;

package body Counters is

    Current : Integer := Start_Value;

    procedure Reset is
    begin
        Current := Start_Value;
    end Reset;

    procedure Bump (By : Integer) is
    begin
        Current := Current + By;
    end Bump;

    function Value return Integer is
    begin
        return Current;
    end Value;

begin
    Ada.Text_IO.Put_Line ("counters elaborated");
end Counters;
