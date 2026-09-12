with Ada.Text_IO; use Ada.Text_IO;

procedure DefaultCalls is
    Counter : Integer := 0;
    Value : Integer := 10;
    function Next_Value return Integer is
    begin
        Counter := Counter + 1;
        return Counter;
    end Next_Value;
    procedure Show (Item : Integer := Next_Value) is
    begin
        Put_Line (Integer'Image (Item));
    end Show;
    procedure Grouped (Left, Right : Integer := Next_Value) is
    begin
        Put_Line (Integer'Image (Left + Right));
    end Grouped;
    function Read_Value (Item : Integer := Value) return Integer is
    begin
        return Item;
    end Read_Value;
    procedure Nested is
        Value : Integer := 99;
    begin
        Put_Line (Integer'Image (Read_Value));
    end Nested;
    procedure Deeper is
        procedure Inner is
        begin
            Put_Line (Integer'Image (Read_Value));
        end Inner;
    begin
        Inner;
    end Deeper;
    type Pair is record
        Left : Integer;
        Right : Integer;
    end record;
    procedure Record_Default (Item : Pair := (3, 4)) is
    begin
        Put_Line (Integer'Image (Item.Left + Item.Right));
    end Record_Default;
    procedure Text (Item : String := "default text") is
    begin
        Put_Line (Item);
    end Text;
    procedure Imported (Item : String := "imported default");
    pragma Import (C, Imported, "__ada_put_line");
    function Imported_Number (Item : Integer := Next_Value) return Integer;
    pragma Import (C, Imported_Number, "abs");
    procedure Partial (Label : String; Item : Integer := Next_Value) is
    begin
        Put_Line (Label & Integer'Image (Item));
    end Partial;
    function Fails return Integer is
    begin
        raise Constraint_Error;
        return 0;
    end Fails;
    procedure Never_Entered (Item : Integer := Fails) is
    begin
        Put_Line ("MISSED default exception");
    end Never_Entered;
    subtype Small is Integer range 1 .. 3;
    procedure Range_Default (Item : Small := Value) is
    begin
        Put_Line ("MISSED default range check");
    end Range_Default;
    procedure Real_Default (Item : Long_Float := 2.5) is
    begin
        if Item /= 2.5 then
            raise Program_Error;
        end if;
        Put_Line ("real default");
    end Real_Default;
    procedure String_Group (Left, Right : String := "group") is
    begin
        Put_Line (Left & Right);
    end String_Group;
    function Wide_Default (Item : Long_Integer := 4294967296) return Long_Integer is
    begin
        return Item;
    end Wide_Default;
    procedure From_Spec (Item : Integer := 42);
    procedure From_Spec (Item : Integer) is
    begin
        Put_Line (Integer'Image (Item));
    end From_Spec;
begin
    Show;
    Show;
    Show (100);
    Put_Line (Integer'Image (Counter));
    Grouped;
    Nested;
    Value := 20;
    Nested;
    Text;
    Imported;
    Partial ("partial");
    Partial (Item => 200, Label => "explicit");
    From_Spec;
    Deeper;
    Record_Default;
    Put_Line (Integer'Image (Imported_Number));
    Real_Default;
    String_Group;
    Put_Line (Long_Integer'Image (Wide_Default));
    begin
        Range_Default;
    exception
        when Constraint_Error => Put_Line ("default range check caught");
    end;
    begin
        Never_Entered;
    exception
        when Constraint_Error => Put_Line ("default exception caught");
    end;
end DefaultCalls;
