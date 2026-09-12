with Ada.Text_IO; use Ada.Text_IO;

procedure ArrayCompare is
    type Vector is array (Integer range <>) of Integer;
    A : Vector (1 .. 3) := (0, 256, -1);
    B : Vector (5 .. 7) := (0, 256, -1);
    C : Vector (1 .. 3) := (0, 256, 255);
    Prefix : Vector (1 .. 2) := (0, 256);
    Empty_A : Vector (10 .. 5);
    Empty_B : Vector (3 .. 1);
    type Wide_Vector is array (1 .. 2) of Long_Integer;
    Wide_A : Wide_Vector := (0, 4294967296);
    Wide_B : Wide_Vector := (0, 8589934592);
    type Real_Vector is array (1 .. 2) of Long_Float;
    Real_A : Real_Vector := (0.0, 1.0);
    Real_B : Real_Vector := (-0.0, 1.0);
    Zero : Long_Float := 0.0;
    type Pair is record
        Code : Character;
        Value : Integer;
    end record;
    type Pairs is array (1 .. 2) of Pair;
    P : Pairs := (('a', 1), ('b', 2));
    Q : Pairs := (('a', 1), ('b', 2));
    type Rows is array (1 .. 2) of Wide_Vector;
    R : Rows := ((1, 2), (3, 4));
    S : Rows := ((1, 2), (3, 4));
    type Colour is (Red, Green, Blue);
    type Colours is array (1 .. 2) of Colour;
    Colour_A : Colours := (Red, Green);
    Colour_B : Colours := (Red, Blue);
    type Signed_Byte is range -128 .. 127;
    for Signed_Byte'Size use 8;
    type Bytes is array (1 .. 2) of Signed_Byte;
    Byte_A : Bytes := (0, -1);
    Byte_B : Bytes := (0, 1);
    type Link is access Integer;
    type Links is array (1 .. 2) of Link;
    L : Links := (null, null);
    M : Links := (null, null);

    procedure Check (Condition : Boolean; Message : String) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
        Put_Line (Message);
    end Check;

    procedure Compare_Parameters (Left, Right : Vector) is
    begin
        Check (Left = Right, "parameter equality ignores lower bounds");
        Check (not (Left /= Right), "parameter inequality complements equality");
    end Compare_Parameters;

    function Less (Left, Right : Vector) return Boolean is
    begin
        return Left < Right;
    end Less;

begin
    Check (A = B, "matching elements with different bounds");
    Check (A /= C, "difference beyond first bytes");
    Check (A < C and C > A, "signed element ordering");
    Check (A <= B and A >= B, "equal arrays include ordering endpoints");
    Check (not (A < B) and not (A > B), "equal arrays exclude strict ordering");
    Check (Prefix < A and A > Prefix, "prefix ordering");
    Check (Prefix /= A, "different lengths are unequal");
    Check (Empty_A = Empty_B, "different null ranges are equal");
    Check (Empty_A < Prefix and Prefix > Empty_B, "null array ordering");
    Compare_Parameters (A, B);
    Compare_Parameters (Empty_A, Empty_B);
    Check (Less (Empty_A, Prefix) and Less (Prefix, A), "runtime prefix and null ordering");
    Check (not Less (Empty_A, Empty_B), "runtime null ranges have equal order");
    Check (A (1 .. 2) = B (5 .. 6), "slice comparison");
    Check (Colour_A < Colour_B, "enumeration element ordering");
    Check (Byte_A < Byte_B, "signed byte element ordering");
    Check (Wide_A /= Wide_B and Wide_A < Wide_B, "64-bit elements");
    Check (Real_A = Real_B, "floating signed zeros are equal");
    Real_B (2) := Zero / Zero;
    Check (Real_B /= Real_B, "NaN element is unequal to itself");
    Check (P = Q, "record elements");
    Q (2).Value := 256;
    Check (P /= Q, "record element difference");
    Check (R = S, "nested array elements");
    S (2)(2) := 5;
    Check (R /= S, "nested array difference");
    Check (L = M, "null access elements");
    L (2) := new Integer'(7);
    M (2) := L (2);
    Check (L = M, "shared access elements");
    M (2) := new Integer'(7);
    Check (L /= M, "access identity rather than designated contents");
end ArrayCompare;
