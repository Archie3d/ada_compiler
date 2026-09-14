with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeArrayTypeChecks is
    N : Integer := 3;
    subtype Index is Integer range 2 .. 4;
    type Vector is array (Index range <>) of Integer;
    type Matrix is array (Index range <>, Index range <>) of Integer;
    subtype Small is Vector (2 .. N);
    subtype Square is Matrix (2 .. N, 2 .. N);
    Count : Integer := 0;
    function Fail return Integer is
    begin
        raise Constraint_Error;
        return 0;
    end Fail;
    procedure Accept_Vector (X : Small) is
    begin
        raise Program_Error;
    end Accept_Vector;
    procedure Accept_Matrix (X : Square) is
    begin
        raise Program_Error;
    end Accept_Matrix;
    function Wrong return Small is
        X : Vector (2 .. 4) := (others => 0);
    begin
        return X;
    end Wrong;
begin
    begin
        declare
            subtype Bad is Vector (1 .. N);
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            type Bad is array (Index range 1 .. N) of Integer;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            subtype Bad is Matrix (2 .. N, 2 .. Fail);
        begin
            raise Program_Error;
        exception
            when Constraint_Error => raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Accept_Vector ((2 .. 4 => 0));
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Accept_Matrix ((2 .. 3 => (2 .. 4 => 0)));
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            X : Small := Wrong;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            X : Small := (others => 0);
        begin
            X (4) := 1;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            X : Small := (others => 0);
        begin
            X := (2 .. 4 => 1);
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    declare
        subtype Empty is Vector (N .. 0);
        X : Empty := (others => Fail);
    begin
        if X'Length /= 0 or Empty'Last /= 0 then
            raise Program_Error;
        end if;
    end;
    -- A type declaration stores bounds without allocating array data.
    declare
        Last : Integer := Integer'Last;
        type Huge is array (1 .. Last, 1 .. Last) of Integer;
    begin
        if Huge'Last (2) /= Last then
            raise Program_Error;
        end if;
        begin
            declare
                X : Huge;
            begin
                raise Program_Error;
            end;
        exception
            when Storage_Error => Count := Count + 1;
        end;
    end;
    begin
        declare
            First : Integer := Integer'First;
            type Wide is array (Integer range <>) of Integer;
            subtype Too_Long is Wide (First .. 0);
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => raise Program_Error;
        when Storage_Error => Count := Count + 1;
    end;
    if Count /= 10 then
        raise Program_Error;
    end if;
    Put_Line ("runtime array type checks ok");
end RuntimeArrayTypeChecks;
