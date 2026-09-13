with Ada.Text_IO;
with Ada.Text_IO.Integer_IO;
with Ada.Text_IO.Float_IO;
with Ada.Text_IO.Enumeration_IO;
with Ada.Float_Text_IO;
use Ada.Text_IO;
use Ada.Float_Text_IO;

procedure IOErrors is

    type Coefficient is digits 8;
    type Day is (Monday, Tuesday);

    type Point is record
        X : Integer;
        Y : Integer;
    end record;

    --  Each generic takes the kind of type its formal part asks for.
    package Bad_Integer is new Ada.Text_IO.Integer_IO (Point);
    package Bad_Float is new Ada.Text_IO.Float_IO (Integer);
    package Bad_Enum is new Ada.Text_IO.Enumeration_IO (Point);
    package Wrong_Formal is new Ada.Text_IO.Integer_IO (Element_Type => Integer);

    C : Coefficient := 1.5;

begin
    --  Float_Text_IO writes a Float, not any floating point type.
    Put (C);
    Put (Day'(Monday));
end IOErrors;
