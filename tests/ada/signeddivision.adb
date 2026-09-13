-- Signed division, rem and mod obey their identities at both integer widths.
with Ada.Text_IO; use Ada.Text_IO;
procedure SignedDivision is
    procedure Check (Condition : Boolean) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
    end Check;
    R, M : Integer;
    Seven : Integer := 7;
    Zero : Integer := 0;
begin
    for A in -7 .. 7 loop
        for B in -3 .. 3 loop
            if B /= 0 then
                R := A rem B;
                M := A mod B;
                Check (A = (A / B) * B + R);
                Check (abs R < abs B and abs M < abs B);
                Check (R = 0 or (R > 0) = (A > 0));
                Check (M = 0 or (M > 0) = (B > 0));
                Check (Long_Integer (A) / Long_Integer (B) = Long_Integer (A / B));
                Check (Long_Integer (A) rem Long_Integer (B) = Long_Integer (R));
                Check (Long_Integer (A) mod Long_Integer (B) = Long_Integer (M));
            end if;
        end loop;
    end loop;
    begin
        R := Seven / Zero;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        R := Seven rem Zero;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    begin
        R := Seven mod Zero;
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    Put_Line ("signeddivision: passed");
end SignedDivision;
