with Ada.Text_IO; use Ada.Text_IO;
with System;

procedure RecordCompare is
    type Kind is (Empty, Small, Large, Other);
    type Payload (Tag : Kind) is record
        Code : Character;
        case Tag is
            when Empty => null;
            when Small => Number : Integer;
            when others => Text : String (1 .. 16);
        end case;
    end record;
    A : Payload (Small);
    B : Payload (Small);
    E : Payload (Empty);
    F : Payload (Empty);
    G : Payload (Large);
    H : Payload (Large);
    I : Payload (Other);
    type Wrapper is record
        Value : Payload (Small);
    end record;
    W : Wrapper;
    X : Wrapper;

    -- Give inactive storage different, deterministic bytes. The aggregates
    -- restore every live component before the objects are compared.
    procedure Fill (Address : System.Address; Byte : Integer; Size : Long_Integer);
    pragma Import (C, Fill, "memset");

    procedure Check (Condition : Boolean; Message : String) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
        Put_Line (Message);
    end Check;

    function Same (Left, Right : Payload) return Boolean is
    begin
        return Left = Right;
    end Same;

begin
    Fill (A'Address, 85, Long_Integer (Payload'Size / 8));
    Fill (B'Address, 170, Long_Integer (Payload'Size / 8));
    A := (Small, 'x', 7);
    B := (Small, 'x', 7);
    Check (A = B, "inactive variant storage is ignored");
    Check (Same (A, B), "runtime discriminant selects active fields");
    B.Number := 8;
    Check (A /= B, "active field difference");
    B.Number := 7;
    B.Code := 'y';
    Check (A /= B, "common field difference");

    Fill (E'Address, 85, Long_Integer (Payload'Size / 8));
    Fill (F'Address, 170, Long_Integer (Payload'Size / 8));
    E := (Empty, 'x');
    F := (Empty, 'x');
    Check (E = F, "empty variant ignores all variant storage");
    Check (not Same (A, E), "different discriminants are unequal");

    G := (Large, 'x', "abcdefghijklmnop");
    H := (Large, 'x', "abcdefghijklmnop");
    I := (Other, 'x', "abcdefghijklmnop");
    Check (G = H, "others alternative equality");
    Check (not Same (G, I), "different discriminants sharing an alternative");
    H.Text (16) := 'z';
    Check (G /= H, "active array component difference");

    W.Value := A;
    X.Value := A;
    Check (W = X, "nested variant record equality");
    X.Value.Number := 9;
    Check (W /= X, "nested variant record difference");
end RecordCompare;
