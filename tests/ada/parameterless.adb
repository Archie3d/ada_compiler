with Ada.Text_IO; use Ada.Text_IO;

package Values is
    type First_Kind is (Shared, First_Only);
    type Second_Kind is (Shared, Second_Only);
    function Pick return Integer;
    function Pick return Boolean;
    procedure With_Default (Item : Integer := 11);
    procedure Action;
    function Action return Integer;
end Values;
package body Values is
    procedure With_Default (Item : Integer) is
    begin
        Put_Line (Integer'Image (Item));
    end With_Default;
    function Pick return Integer is
    begin
        return 9;
    end Pick;
    function Pick return Boolean is
    begin
        return True;
    end Pick;
    procedure Action is
    begin
        Put_Line ("procedure context");
    end Action;
    function Action return Integer is
    begin
        return 10;
    end Action;
end Values;

procedure Parameterless is
    function Pick (Required : Integer) return Integer is
    begin
        return Required;
    end Pick;
    function Pick return Integer is
    begin
        return 7;
    end Pick;
    function Pick return Boolean is
    begin
        return True;
    end Pick;
    function Defaulted (N : Integer := 8) return Integer is
    begin
        return N;
    end Defaulted;
    function Defaulted (N : Boolean := True) return Boolean is
    begin
        return N;
    end Defaulted;
    N : Integer;
    B : Boolean;
    E : Values.Second_Kind := Values.Shared;
begin
    N := Pick;
    B := Pick;
    Put_Line (Integer'Image (N));
    if B then
        Put_Line ("Boolean context");
    end if;
    N := Defaulted;
    B := Defaulted;
    Put_Line (Integer'Image (N));
    if B then
        Put_Line ("defaulted Boolean context");
    end if;
    N := Values.Pick;
    B := Values.Pick;
    Put_Line (Integer'Image (N));
    if B then
        Put_Line ("qualified Boolean context");
    end if;
    Values.Action;
    N := Values.Action;
    Put_Line (Integer'Image (N));
    Values.With_Default;
    Put_Line (Values.Second_Kind'Image (Values.Shared));
    if E = Values.Shared then
        Put_Line ("qualified enumeration context");
    end if;
end Parameterless;
