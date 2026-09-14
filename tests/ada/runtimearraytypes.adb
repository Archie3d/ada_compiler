with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeArrayTypes is
    Calls : Integer := 0;
    function Bound (N : Integer) return Integer is
    begin
        Calls := Calls + 1;
        return N;
    end Bound;

    procedure Check (N : Integer) is
        Limit : Integer := N;
        subtype Text is String (2 .. Bound (Limit));
        subtype Alias_Text is Text;
        type Vector is array (3 .. Bound (Limit + 1)) of Integer;
        type Matrix is array (Integer range <>, Integer range <>) of Integer;
        subtype Shape is Matrix (2 .. Bound (Limit), 5 .. Bound (Limit + 3));
        type Fixed_Matrix is array (1 .. 2, 4 .. Bound (Limit + 2)) of Integer;
        A : Alias_Text := (others => 'a');
        V : Vector := (others => 7);
        M : Shape := (others => (others => 9));
        F : Fixed_Matrix := (others => (others => 8));

        procedure Inspect (X : Text; Y : Shape) is
            Copy : Alias_Text := X;
        begin
            if X'First /= 2 or X'Last /= N or Copy /= A
                or Y'First (2) /= 5 or Y'Last (2) /= N + 3
                or Text'Last /= N or Shape'Length (2) /= N - 1 then
                raise Program_Error;
            end if;
        end Inspect;

        function Make return Shape is
        begin
            return (others => (others => 9));
        end Make;

        function Slide return Text is
        begin
            return (11 .. N + 9 => 'a');
        end Slide;
    begin
        Limit := 100;
        declare
            Later : Text;
            Later_Matrix : Shape := Make;
        begin
            Later := Slide;
            if Later /= A or Later_Matrix /= M or Vector'Length /= N - 1
                or F'Length (1) /= 2 or F'Length (2) /= N - 1 then
                raise Program_Error;
            end if;
            Inspect ((11 .. N + 9 => 'a'), (7 .. N + 5 => (9 .. N + 7 => 9)));
        end;
        for I in Text'Range loop
            if A (I) /= 'a' then
                raise Program_Error;
            end if;
        end loop;
        if N > 2 then
            Check (N - 1);
        end if;
        if Text'Last /= N or Shape'Last (2) /= N + 3 or V (3) /= 7 then
            raise Program_Error;
        end if;
    end Check;
begin
    Check (4);
    if Calls /= 15 then
        raise Program_Error;
    end if;
    for N in 1 .. 3 loop
        declare
            subtype Text is String (4 .. Bound (N));
            type Empty is array (4 .. N, 1 .. 2) of Integer;
            A : Text := (others => 'x');
            B : Empty := (others => (others => 0));
        begin
            if A'Length /= 0 or Text'First /= 4 or Text'Last /= N
                or B'Length (1) /= 0 or Empty'Length (2) /= 2 then
                raise Program_Error;
            end if;
        end;
    end loop;
    if Calls /= 18 then
        raise Program_Error;
    end if;
    declare
        type Day is (Mon, Tue, Wed, Thu);
        Last_Day : Day := Wed;
        type Week;
        type Week is array (Day range Mon .. Last_Day) of Integer;
        W : Week := (others => 4);
        N : Integer := 3;
        package Local is
            subtype Text is String (2 .. N);
            A : Text := (others => 'p');
            function Make return Text;
            type Hidden is private;
            function Make_Hidden return Hidden;
        private
            type Hidden is array (1 .. Bound (N)) of Integer;
        end Local;
        package body Local is
            function Make return Text is
            begin
                return (others => 'p');
            end Make;
            function Make_Hidden return Hidden is
            begin
                return (others => 5);
            end Make_Hidden;
        end Local;
        procedure Nested is
            B : Local.Text := Local.Make;
            H : Local.Hidden := Local.Make_Hidden;
        begin
            if B /= Local.A or Local.Text'First /= 2 or Local.Text'Last /= 3 then
                raise Program_Error;
            end if;
        end Nested;
    begin
        N := 9;
        Last_Day := Thu;
        Nested;
        if Week'Last /= Wed or W (Wed) /= 4 then
            raise Program_Error;
        end if;
    end;
    if Calls /= 19 then
        raise Program_Error;
    end if;
    Put_Line ("runtime array types ok");
end RuntimeArrayTypes;
