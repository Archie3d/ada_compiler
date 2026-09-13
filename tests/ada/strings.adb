with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Strings is

    Greeting : String (1 .. 5) := "hello";
    Line     : String (1 .. 11) := "hello world";
    Copy     : String (1 .. 5);

    procedure Describe (Text : String) is
    begin
        Put (Text);
        Put (" first=");
        Put (Text'First, 1);
        Put (" last=");
        Put (Text'Last, 1);
        Put (" length=");
        Put (Text'Length, 1);
        New_Line;
    end Describe;

    procedure Outer (Text : String) is

        procedure Inner is
        begin
            Put ("inner sees ");
            Put (Text);
            Put (Text'Length, 3);
            New_Line;
        end Inner;

    begin
        Inner;
    end Outer;

begin
    Describe (Greeting);
    Describe ("a literal");
    Describe (Line (7 .. 11));

    Outer (Line);

    Put_Line (Line (1 .. 5));
    Put_Line (Greeting & ", " & "ada" & '!');
    Put_Line ("count:" & Integer'Image (Line'Length));

    Copy := Line (7 .. 11);
    Put_Line (Copy);

    if Greeting = "hello" then
        Put_Line ("equal");
    end if;
    if Greeting /= Line (1 .. 5) then
        Put_Line ("unexpected");
    end if;
    if "abc" < "abd" then
        Put_Line ("less");
    end if;
    if "abc" < "abcd" then
        Put_Line ("prefix is less");
    end if;

    for I in 1 .. 3 loop
        Put_Line (Line (I .. I + 4));
    end loop;

    declare
        Count : Integer := 4;
    begin
        Copy := Line (1 .. Count);
        Put_Line ("no check");
    exception
        when Constraint_Error =>
            Put_Line ("length mismatch");
    end;
end Strings;
