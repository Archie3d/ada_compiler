with Ada.Text_IO; use Ada.Text_IO;
package State is
    Value : Integer := 1;
    function Observe (Expected : Integer; Message : String) return Integer;
end State;
package body State is
    function Observe (Expected : Integer; Message : String) return Integer is
    begin
        if Value /= Expected then
            raise Program_Error;
        end if;
        Put_Line (Message);
        return Value;
    end Observe;
begin
    Value := 2;
    Put_Line ("state body");
end State;
package Later is
    Seen : Integer := State.Observe (2, "later specification");
private
    Hidden : Integer := State.Observe (2, "later private part");
end Later;
package body Later is
    Before : Integer := State.Observe (2, "before nested package");
    package Inner is
        Seen : Integer := State.Observe (2, "nested specification");
    end Inner;
    package body Inner is
    begin
        State.Value := 3;
        Put_Line ("nested body");
    end Inner;
    After : Integer := State.Observe (3, "after nested package");
    type Record_Value is record
        Value : Integer := State.Observe (3, "component default");
    end record;
    Item : Record_Value;
begin
    State.Value := 4;
    Put_Line ("later body");
end Later;
procedure ElaborationOrder is
begin
    if State.Value /= 4 or Later.Seen /= 2 then
        raise Program_Error;
    end if;
    Put_Line ("main");
end ElaborationOrder;
