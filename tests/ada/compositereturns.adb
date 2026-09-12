with Ada.Text_IO; use Ada.Text_IO;

package Return_Values is
    function Text return String;
end Return_Values;

package body Return_Values is
    function Text return String is
    begin
        return "library";
    end Text;
end Return_Values;

procedure CompositeReturns is
    type Pair is record
        Number : Integer;
        Text : String (1 .. 4);
    end record;
    type Vector is array (Integer range <>) of Integer;
    subtype Triple is Vector (4 .. 6);

    procedure Check (Condition : Boolean; Message : String) is
    begin
        if not Condition then
            raise Program_Error;
        end if;
        Put_Line (Message);
    end Check;

    function Make (Number : Integer) return Pair is
        Local : Pair := (Number, "safe");
    begin
        return Local;
    end Make;

    function Aggregate_Pair return Pair is
    begin
        return (17, "aggr");
    end Aggregate_Pair;

    procedure Default_Pair (Value : Pair := Make (27)) is
    begin
        Check (Value.Number = 27, "composite default result");
    end Default_Pair;

    function Recur (Depth : Integer) return Pair is
    begin
        if Depth = 0 then
            return Make (42);
        end if;
        return Recur (Depth - 1);
    end Recur;

    function Fixed (Number : Integer) return Triple is
        Local : Triple := (Number, Number + 1, Number + 2);
    begin
        return Local;
    end Fixed;

    function Copy (Source : Vector) return Vector is
    begin
        return Source;
    end Copy;

    function Words (Source : String) return String is
        Local : String (7 .. 10) := "word";
    begin
        if Source'Length = 0 then
            return Local;
        end if;
        return Source;
    end Words;

    function Decorate (Source : String) return String is
    begin
        return "[" & Words (Source) & "]";
    end Decorate;

    procedure Bounds (Source : String; First, Last : Integer) is
    begin
        Check (Source'First = First and Source'Last = Last, "returned bounds");
    end Bounds;

    function Broken return Pair is
    begin
        raise Constraint_Error;
        return Make (0);
    end Broken;

    function Bad_Length (Source : Vector) return Triple is
    begin
        return Source;
    end Bad_Length;

    function Failed_Array return Vector is
    begin
        raise Constraint_Error;
        return Fixed (0);
    end Failed_Array;

    function Missing return Vector is
    begin
        null;
    end Missing;

    A : Pair := Recur (8);
    V : Triple := Fixed (5);
    S : String (1 .. 4) := Words ("");
    Empty : String (9 .. 3);
begin
    Check (Return_Values.Text = "library", "library function result");
    Check (Copy (Fixed (1)) (4) + Copy (Fixed (2)) (4) = 3, "independent array temporaries");
    Check (A.Number = 42 and A.Text = "safe", "record initialization and recursion");
    Check (Make (1).Number + Make (2).Number = 3, "independent record temporaries");
    Check (Aggregate_Pair.Text = "aggr", "record aggregate result");
    Default_Pair;
    A := Make (99);
    Check (A.Number = 99, "record assignment");
    Check (V (4) = 5 and V (6) = 7, "constrained array initialization");
    V := Copy (Fixed (10));
    Check (V (4) = 10 and V (6) = 12, "unconstrained array assignment and nesting");
    Check (Copy (V) (5) = 11, "returned array indexing");
    Check (S = "word", "returned string initialization with sliding");
    Bounds (Words (""), 7, 10);
    Bounds (Words (Empty), 7, 10);
    Bounds (Words (S (3 .. 2)), 7, 10);
    Check (Decorate ("abc") = "[abc]", "return concatenation and nested calls");
    declare
        Null_Vector : Vector (8 .. 2);
    begin
        Check (Copy (Null_Vector)'Length = 0, "null array length");
        Check (Copy (Null_Vector)'First = 8 and Copy (Null_Vector)'Last = 2, "null array bounds");
    end;
    begin
        S := Decorate ("abc");
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("result length check");
    end;
    begin
        A := Broken;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("failed result is not consumed");
    end;
    declare
        Short : Vector (1 .. 2) := (1, 2);
    begin
        V := Bad_Length (Short);
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("constrained return length check");
    end;
    begin
        V := Failed_Array;
        raise Program_Error;
    exception
        when Constraint_Error => Put_Line ("failed array descriptor is not consumed");
    end;
    begin
        V := Missing;
        raise Constraint_Error;
    exception
        when Program_Error => Put_Line ("missing return raises");
    end;
end CompositeReturns;
