procedure BaseErrors is

   Base_Too_Large : constant Integer := 20#12#;
   Digit_Too_Big  : constant Integer := 2#12#;
   Unterminated   : constant Integer := 16#FF;
   Negative_Power : constant Integer := 16#1#E-2;
   Way_Too_Large  : constant Integer := 16#FFFF_FFFF_FFFF_FFFF#;
   No_Fraction    : constant Float   := 16#1.#;

begin
   null;
end BaseErrors;
