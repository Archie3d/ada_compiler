with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Cases is

    type Sensor_Kind is (Elevation, Azimuth, Distance, Spare);
    type Day is (Mon, Tue, Wed, Thu, Fri, Sat, Sun);
    subtype Weekday is Day range Mon .. Fri;

    Error : exception;

    Sensor_Value : Integer := 42;

    procedure Record_Elevation (Value : in Integer) is
    begin
        Put ("elevation");
        Put (Value, 4);
        New_Line;
    end Record_Elevation;

    procedure Record_Azimuth (Value : in Integer) is
    begin
        Put ("azimuth  ");
        Put (Value, 4);
        New_Line;
    end Record_Azimuth;

    procedure Record_Distance (Value : in Integer) is
    begin
        Put ("distance ");
        Put (Value, 4);
        New_Line;
    end Record_Distance;

    -- Every alternative names one value, and 'others' takes what is left.
    procedure Read (Sensor : in Sensor_Kind) is
    begin
        case Sensor is
            when Elevation => Record_Elevation (Sensor_Value);
            when Azimuth   => Record_Azimuth (Sensor_Value);
            when Distance  => Record_Distance (Sensor_Value);
            when others    => null;
        end case;
    end Read;

    procedure Compute_Initial_Balance is
    begin
        Put_Line ("initial balance");
    end Compute_Initial_Balance;

    procedure Compute_Closing_Balance is
    begin
        Put_Line ("closing balance");
    end Compute_Closing_Balance;

    procedure Generate_Report (Today : in Day) is
    begin
        Put ("report for ");
        Put_Line (Day'Image (Today));
    end Generate_Report;

    -- Single values and ranges together, covering the seven days between them,
    -- which is why this one needs no 'others'.
    procedure Post (Today : in Day) is
    begin
        case Today is
            when Mon        => Compute_Initial_Balance;
            when Fri        => Compute_Closing_Balance;
            when Tue .. Thu => Generate_Report (Today);
            when Sat .. Sun => null;
        end case;
    end Post;

    procedure Update_Bin (N : in Integer) is
    begin
        Put ("update");
        Put (N, 3);
        New_Line;
    end Update_Bin;

    procedure Empty_Bin (N : in Integer) is
    begin
        Put ("empty ");
        Put (N, 3);
        New_Line;
    end Empty_Bin;

    function Bin_Number (Count : in Integer) return Integer is
    begin
        return Count;
    end Bin_Number;

    -- A call as the selector, a choice naming two values, an alternative with
    -- several statements, and 'others' raising rather than falling through.
    procedure Sort (Count : in Integer) is
    begin
        case Bin_Number (Count) is
            when 1 => Update_Bin (1);
            when 2 => Update_Bin (2);
            when 3 | 4 =>
                Empty_Bin (1);
                Empty_Bin (2);
            when others => raise Error;
        end case;
    end Sort;

    -- A subtype mark stands for every value the subtype holds.
    procedure Classify (Today : in Day) is
    begin
        case Today is
            when Weekday => Put_Line (Day'Image (Today) & " works");
            when others  => Put_Line (Day'Image (Today) & " rests");
        end case;
    end Classify;

    -- Characters are discrete too, and a case may sit inside a case.
    function Score (Letter : in Character) return Integer is
        Value : Integer;
    begin
        case Letter is
            when 'a' | 'e' | 'i' | 'o' | 'u' =>
                Value := 1;
            when 'b' .. 'd' =>
                case Letter is
                    when 'b'    => Value := 20;
                    when others => Value := 30;
                end case;
            when others =>
                Value := 0;
        end case;
        return Value;
    end Score;

begin
    Put_Line ("Sensors");
    for S in Sensor_Kind loop
        Read (S);
    end loop;

    Put_Line ("Days");
    for D in Day loop
        Post (D);
    end loop;

    Put_Line ("Bins");
    for I in 1 .. 4 loop
        Sort (I);
    end loop;
    begin
        Sort (9);
    exception
        when Error =>
            Put_Line ("no such bin");
    end;

    Put_Line ("Working");
    for D in Day loop
        Classify (D);
    end loop;

    Put_Line ("Letters");
    for C in Character range 'a' .. 'f' loop
        Put (C);
        Put (Score (C), 3);
        New_Line;
    end loop;
end Cases;
