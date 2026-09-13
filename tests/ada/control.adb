with Ada.Text_IO;
with Ada.Integer_Text_IO;
use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Control is
    Total : Integer := 0;
    Count : Integer;
begin
    for I in 1 .. 5 loop
        Total := Total + I;
    end loop;
    Put ("sum:");
    Put (Total, 3);
    New_Line;

    Put ("countdown:");
    for I in reverse 1 .. 5 loop
        Put (I, 2);
    end loop;
    New_Line;

    Count := 0;
    while Count < 3 loop
        Count := Count + 1;
    end loop;
    Put ("while:");
    Put (Count, 3);
    New_Line;

    Count := 0;
    loop
        Count := Count + 2;
        exit when Count >= 7;
    end loop;
    Put ("loop:");
    Put (Count, 3);
    New_Line;

    Outer :
    for I in 1 .. 3 loop
        for J in 1 .. 3 loop
            if I * J > 4 then
                exit Outer;
            end if;
            Total := I * 10 + J;
        end loop;
    end loop Outer;
    Put ("labelled:");
    Put (Total, 4);
    New_Line;

    for I in 1 .. 5 loop
        case I is
            when 1      => Put_Line ("one");
            when 2 | 3  => Put_Line ("two or three");
            when 4 .. 5 => Put_Line ("four or five");
            when others => Put_Line ("other");
        end case;
    end loop;

    if Total > 100 then
        Put_Line ("big");
    elsif Total > 10 then
        Put_Line ("medium");
    else
        Put_Line ("small");
    end if;
end Control;
