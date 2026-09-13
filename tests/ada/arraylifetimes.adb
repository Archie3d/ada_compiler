with Ada.Text_IO; use Ada.Text_IO;
procedure ArrayLifetimes is
    Size : Integer := 200;
    Original : exception;
    function Make (N : Integer) return String is
        Text : String (1 .. N) := (others => 'x');
    begin
        return Text;
    end Make;
    function Wrap (Text : String) return String is
    begin
        return "[" & Text & "]";
    end Wrap;
    function Fail return String is
        Text : String := Make (Size);
    begin
        raise Original;
        return Text;
    end Fail;
    function Early (N : Integer) return String is
    begin
        declare
            Text : String := Make (N);
        begin
            return Wrap (Text);
        end;
    end Early;
    Count : Integer := 0;
begin
    for I in 1 .. 200 loop
        declare
            Outer : String := Make (Size);
        begin
            begin
                declare
                    Inner : String := Wrap (Outer);
                begin
                    raise Original;
                end;
            exception
                when Original =>
                    if Outer'Length /= Size or Outer (1) /= 'x' then
                        raise Program_Error;
                    end if;
            end;
            begin
                declare
                    Inner : String := Fail;
                begin
                    raise Program_Error;
                end;
            exception
                when Original => null;
            end;
        end;
    end loop;
    Put_Line ("abandoned blocks reclaimed; enclosing arrays preserved");
    declare
        Kept : String := Make (Size);
    begin
        raise Original;
    exception
        when Original =>
            if Kept (1) /= 'x' then
                raise Program_Error;
            end if;
            Put_Line ("block locals survive their handler");
    end;
    Outer_Loop : loop
        declare
            Text : String := Make (Size);
        begin
            loop
                declare
                    More : String := Wrap (Text);
                begin
                    exit Outer_Loop when More'Length = Size + 2;
                end;
            end loop;
        end;
    end loop Outer_Loop;
    Put_Line ("labelled exit leaves nested allocations");
    while Make (Count)'Length < 100 loop
        if Wrap (Make (Size)) /= Early (Size) then
            raise Program_Error;
        end if;
        Count := Count + 1;
    end loop;
    Put_Line ("loop condition and expression temporaries");
    if Early (0) /= "[]" then
        raise Program_Error;
    end if;
    Put_Line ("returned arrays outlive callee cleanup");
end ArrayLifetimes;
