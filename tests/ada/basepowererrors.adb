procedure Basepowererrors is
    type Vector is array (1 .. 2) of Integer;
    subtype Bad is Vector'Base;
    Value : Integer := 1;
    subtype Object_Base is Value'Base;
    function "**" (Left : Integer) return Integer;
    function "**" (Left : in out Integer; Right : Integer) return Integer;
    function "**" (Left : Integer; Right : Integer := 1) return Integer;
    F : Float;
begin
    F := 2.0 ** 0.5;
    Value := Value'Base'First;
end Basepowererrors;
