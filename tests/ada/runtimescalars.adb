with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeScalars is
    Calls : Integer := 0;
    function Bound (N : Integer) return Integer is
    begin
        Calls := Calls + 1;
        return N;
    end Bound;
    procedure Check (N : Integer) is
        Limit : Integer := N;
        subtype Index is Integer range Bound (2) .. Bound (Limit);
        subtype Alias_Index is Index;
        subtype Narrow is Index range 2 .. 3;
        type Vector is array (Index) of Integer;
        type Matrix is array (Index, Index) of Integer;
        type Open_Vector is array (Index range <>) of Integer;
        type Open_Matrix is array (Index range <>, Index range <>) of Integer;
        type Partial is array (Index range 2 .. 3) of Integer;
        A : Vector := (others => 7);
        M : Matrix := (others => (others => 8));
        P : Partial := (others => 9);
        X : Alias_Index := N;
        Sum : Integer := 0;
        function Echo (Value : Index := Index'Last) return Alias_Index is
        begin
            return Value;
        end Echo;
        procedure Inner is
            Y : Index := Echo;
            B : Open_Vector := (1, 2);
            C : Open_Matrix := ((1, 2), (3, 4));
        begin
            if Y /= N or Index'Last /= N or B'First /= 2 or B'Last /= 3
                or C'First (1) /= 2 or C'Last (2) /= 3 or C (3, 3) /= 4 then
                raise Program_Error;
            end if;
        end Inner;
    begin
        Limit := 100;
        Inner;
        X := Narrow (3);
        if X /= 3 or N not in Alias_Index or 1 in Index
            or Index'Base'Last /= Integer'Last then
            raise Program_Error;
        end if;
        for I in Index loop
            Sum := Sum + A (I);
        end loop;
        for I in reverse Alias_Index'Range loop
            Sum := Sum - A (I);
        end loop;
        if Sum /= 0 or Vector'First /= 2 or Vector'Last /= N
            or Matrix'Length (2) /= N - 1 or M (2, 2) /= 8 or P (3) /= 9 then
            raise Program_Error;
        end if;
        if N > 3 then
            Check (N - 1);
        end if;
        if Index'Last /= N or Echo (N) /= N then
            raise Program_Error;
        end if;
    end Check;
begin
    Check (4);
    if Calls /= 4 then
        raise Program_Error;
    end if;
    for N in 1 .. 3 loop
        declare
            subtype Index is Integer range 1 .. N;
            type Vector is array (Index) of Integer;
            A : Vector := (others => 1);
        begin
            if A'Length /= N or Index'Last /= N then
                raise Program_Error;
            end if;
        end;
    end loop;
    declare
        type Day is (Mon, Tue, Wed, Thu);
        Last_Day : Day := Wed;
        subtype Days is Day range Tue .. Last_Day;
        subtype Alias_Days is Days;
        type Week is array (Days) of Integer;
        W : Week := (others => 5);
        Count : Integer := 0;
    begin
        Last_Day := Thu;
        if Days'First /= Tue or Alias_Days'Last /= Wed or Thu in Days then
            raise Program_Error;
        end if;
        for D in Days loop
            Count := Count + W (D);
        end loop;
        if Count /= 10 then
            raise Program_Error;
        end if;
    end;
    declare
        Last : Long_Integer := 4_000_000_002;
        subtype Wide is Long_Integer range 4_000_000_000 .. Last;
        subtype Alias_Wide is Wide;
        X : Wide := Last;
        Count : Integer := 0;
        function Echo (Y : Wide) return Wide is
        begin
            return Y;
        end Echo;
    begin
        Last := 9;
        if Wide'Last /= 4_000_000_002 or Echo (X) /= Wide'Last
            or 4_000_000_001 not in Alias_Wide or 1 in Wide then
            raise Program_Error;
        end if;
        for I in reverse Wide loop
            Count := Count + 1;
        end loop;
        if Count /= 3 then
            raise Program_Error;
        end if;
    end;
    declare
        N : Integer := 0;
        subtype Empty is Positive range 2 .. N;
        type Empty_Vector is array (Empty) of Integer;
        X : Empty_Vector := (others => 0);
    begin
        if Empty'First /= 2 or Empty'Last /= 0 or 1 in Empty or X'Length /= 0 then
            raise Program_Error;
        end if;
        for I in Empty loop
            raise Program_Error;
        end loop;
    end;
    declare
        Upper : Long_Integer := Long_Integer'Last;
        subtype Tail is Long_Integer range Upper - 1 .. Upper;
        Count : Integer := 0;
        package Local is
            subtype Last_Value is Tail range Upper .. Upper;
            function Last return Last_Value;
        end Local;
        package body Local is
            function Last return Last_Value is
            begin
                return Last_Value'Last;
            end Last;
        end Local;
        procedure Nested is
        begin
            if Local.Last /= Long_Integer'Last or Local.Last_Value'First /= Long_Integer'Last then
                raise Program_Error;
            end if;
        end Nested;
    begin
        Upper := 1;
        Nested;
        for I in Tail loop
            Count := Count + 1;
        end loop;
        if Count /= 2 then
            raise Program_Error;
        end if;
    end;
    Put_Line ("runtime scalars ok");
end RuntimeScalars;
