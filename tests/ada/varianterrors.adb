-- What a discriminated record refuses.

procedure Varianterrors is

    type Figure is (Point, Circle, Rectangle);

    type Shape (Kind : Figure) is
        record
            Label : Character;
            case Kind is
                when Point =>
                    null;
                when Circle =>
                    Radius : Integer;
                when Rectangle =>
                    Width  : Integer;
                    Height : Integer;
            end case;
        end record;

    -- A discriminant is fixed when the object is declared, so a declaration has
    -- to say what to fix it to.
    Loose : Shape;

    C : Shape (Circle) := (Circle, 'c', 5);
    R : Shape (Rectangle) := (Rectangle, 'r', 3, 4);

    -- Only the alternative the discriminant picks is there, so the constraint
    -- and the aggregate have to agree about which one that is.
    Wrong : Shape (Circle) := (Kind => Rectangle, Label => 'w', Width => 1, Height => 2);

    -- The discriminant has to be fixed with a value it can take, and with one
    -- value rather than a range.
    Ranged : Shape (Circle .. Rectangle);

begin
    -- Reaching for a component of another alternative than the one the subtype
    -- settled on.
    C.Width := 3;

    -- The discriminant itself was fixed when the object was declared.
    R.Kind := Circle;

    -- An aggregate for the wrong alternative.
    C := (Circle, 'c', Width => 1, Height => 2);

end Varianterrors;
