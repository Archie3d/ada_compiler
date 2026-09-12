with Ada.Text_IO; use Ada.Text_IO;
procedure Overloads is
    function Pick (N : Integer) return Integer;
    function Pick (N : Integer) return Boolean;
    -- Define the bodies in reverse order to check profile matching.
    function Pick (N : Integer) return Boolean is
    begin
        return N = 1;
    end Pick;
    function Pick (N : Integer) return Integer is
    begin
        return N + 10;
    end Pick;
    procedure Consume (Flag : Boolean) is
    begin
        if Flag then
            Put_Line ("nested result context");
        end if;
    end Consume;
    X : Integer;
    B : Boolean;
begin
    X := Pick (1);
    B := Pick (N => 1);
    Put_Line (Integer'Image (X));
    if B then
        Put_Line ("Boolean result");
    end if;
    Consume (Pick (1));
    declare
        function Pick (N : Integer) return Integer is
        begin
            return N + 20;
        end Pick;
    begin
        X := Pick (1);
        Put_Line (Integer'Image (X));
    end;
end Overloads;
