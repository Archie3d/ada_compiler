with Ada.Text_IO; use Ada.Text_IO;
procedure DynamicArrays is
    Calls : Integer := 0;
    function Text return String is
    begin
        Calls := Calls + 1;
        return "abc";
    end Text;
    procedure Run (Low, High : Integer) is
        Buffer : String (Low .. High) := (others => 'x');
        Saved : String := Text;
        function Read_Back return String is
        begin
            return Buffer;
        end Read_Back;
        procedure Change is
        begin
            if Buffer'Length > 0 then
                Buffer (Low) := 'y';
            end if;
        end Change;
    begin
        if Buffer'First /= Low or Buffer'Last /= High then
            raise Program_Error;
        end if;
        Change;
        if Buffer'Length > 0 then
            if Buffer (Low) /= 'y' then
                raise Program_Error;
            end if;
        end if;
        if Read_Back /= Buffer then
            raise Program_Error;
        end if;
        Saved := "def";
        if Saved /= "def" or Saved'Length /= 3 then
            raise Program_Error;
        end if;
        begin
            Saved := "long";
            raise Program_Error;
        exception
            when Constraint_Error => Put_Line ("dynamic assignment length check");
        end;
        begin
            Buffer (High + 1) := 'z';
            raise Program_Error;
        exception
            when Constraint_Error => Put_Line ("dynamic index check");
        end;
    end Run;
begin
    Run (4, 8);
    Run (7, 2);
    if Calls /= 2 then
        raise Program_Error;
    end if;
    Put_Line ("bounds, captures, null arrays, and single initialization");
end DynamicArrays;
