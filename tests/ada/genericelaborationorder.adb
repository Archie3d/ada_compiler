with Ada.Text_IO; use Ada.Text_IO;
package Context_State is
    Value : Integer := 1;
end Context_State;
package body Context_State is
begin
    Value := 5;
    Put_Line ("context ready");
end Context_State;
generic
package Template is
    Seen : Integer := Context_State.Value;
end Template;
package body Template is
begin
    if Seen /= 5 then
        raise Program_Error;
    end if;
    Context_State.Value := 6;
    Put_Line ("instance body");
end Template;
package Instance is new Template;
package After_Instance is
    Seen : Integer := Context_State.Value;
end After_Instance;
procedure GenericElaborationOrder is
begin
    if Instance.Seen /= 5 or After_Instance.Seen /= 6 then
        raise Program_Error;
    end if;
    Put_Line ("main after instance");
end GenericElaborationOrder;
