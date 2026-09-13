with Ada.Numerics;

package body Ada.Numerics.Generic_Elementary_Functions is

    function C_Sqrt (X : Long_Float) return Long_Float;
    pragma Import (C, C_Sqrt, "__ada_numerics_sqrt");

    function C_Log (X : Long_Float) return Long_Float;
    pragma Import (C, C_Log, "__ada_numerics_log");

    function C_Exp (X : Long_Float) return Long_Float;
    pragma Import (C, C_Exp, "__ada_numerics_exp");

    function C_Sin (X : Long_Float) return Long_Float;
    pragma Import (C, C_Sin, "__ada_numerics_sin");

    function C_Cos (X : Long_Float) return Long_Float;
    pragma Import (C, C_Cos, "__ada_numerics_cos");

    function C_Tan (X : Long_Float) return Long_Float;
    pragma Import (C, C_Tan, "__ada_numerics_tan");

    function C_Cot (X : Long_Float) return Long_Float;
    pragma Import (C, C_Cot, "__ada_numerics_cot");

    function C_Arcsin (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arcsin, "__ada_numerics_arcsin");

    function C_Arccos (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arccos, "__ada_numerics_arccos");

    function C_Sinh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Sinh, "__ada_numerics_sinh");

    function C_Cosh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Cosh, "__ada_numerics_cosh");

    function C_Tanh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Tanh, "__ada_numerics_tanh");

    function C_Coth (X : Long_Float) return Long_Float;
    pragma Import (C, C_Coth, "__ada_numerics_coth");

    function C_Arcsinh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arcsinh, "__ada_numerics_arcsinh");

    function C_Arccosh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arccosh, "__ada_numerics_arccosh");

    function C_Arctanh (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arctanh, "__ada_numerics_arctanh");

    function C_Arccoth (X : Long_Float) return Long_Float;
    pragma Import (C, C_Arccoth, "__ada_numerics_arccoth");

    function C_Log_Base (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Log_Base, "__ada_numerics_log_base");

    function C_Power (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Power, "__ada_numerics_power");

    function C_Sin_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Sin_Cycle, "__ada_numerics_sin_cycle");

    function C_Cos_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Cos_Cycle, "__ada_numerics_cos_cycle");

    function C_Tan_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Tan_Cycle, "__ada_numerics_tan_cycle");

    function C_Cot_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Cot_Cycle, "__ada_numerics_cot_cycle");

    function C_Arcsin_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Arcsin_Cycle, "__ada_numerics_arcsin_cycle");

    function C_Arccos_Cycle (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Arccos_Cycle, "__ada_numerics_arccos_cycle");

    function C_Arctan (X, Y : Long_Float) return Long_Float;
    pragma Import (C, C_Arctan, "__ada_numerics_arctan");

    function C_Arctan_Cycle (X, Y, Cycle : Long_Float) return Long_Float;
    pragma Import (C, C_Arctan_Cycle, "__ada_numerics_arctan_cycle");

    function Sqrt (X : Float_Type'Base) return Float_Type'Base is
    begin
        if X < 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Sqrt (Long_Float (X)));
    end Sqrt;

    function Log (X : Float_Type'Base) return Float_Type'Base is
    begin
        if X < 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Log (Long_Float (X)));
    end Log;

    function Exp (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Exp (Long_Float (X)));
    end Exp;

    function Sin (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Sin (Long_Float (X)));
    end Sin;

    function Cos (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Cos (Long_Float (X)));
    end Cos;

    function Tan (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Tan (Long_Float (X)));
    end Tan;

    function Cot (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Cot (Long_Float (X)));
    end Cot;

    function Arcsin (X : Float_Type'Base) return Float_Type'Base is
    begin
        if abs X > 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arcsin (Long_Float (X)));
    end Arcsin;

    function Arccos (X : Float_Type'Base) return Float_Type'Base is
    begin
        if abs X > 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arccos (Long_Float (X)));
    end Arccos;

    function Sinh (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Sinh (Long_Float (X)));
    end Sinh;

    function Cosh (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Cosh (Long_Float (X)));
    end Cosh;

    function Tanh (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Tanh (Long_Float (X)));
    end Tanh;

    function Coth (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Coth (Long_Float (X)));
    end Coth;

    function Arcsinh (X : Float_Type'Base) return Float_Type'Base is
    begin
        return Float_Type'Base (C_Arcsinh (Long_Float (X)));
    end Arcsinh;

    function Arccosh (X : Float_Type'Base) return Float_Type'Base is
    begin
        if X < 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arccosh (Long_Float (X)));
    end Arccosh;

    function Arctanh (X : Float_Type'Base) return Float_Type'Base is
    begin
        if abs X > 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arctanh (Long_Float (X)));
    end Arctanh;

    function Arccoth (X : Float_Type'Base) return Float_Type'Base is
    begin
        if abs X < 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arccoth (Long_Float (X)));
    end Arccoth;

    function Log (X, Base : Float_Type'Base) return Float_Type'Base is
    begin
        if X < 0.0 or Base <= 0.0 or Base = 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Log_Base (Long_Float (X), Long_Float (Base)));
    end Log;

    function "**" (Left, Right : Float_Type'Base) return Float_Type'Base is
    begin
        if Left < 0.0 or (Left = 0.0 and Right = 0.0) then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Power (Long_Float (Left), Long_Float (Right)));
    end "**";

    function Sin (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Sin_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Sin;

    function Cos (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Cos_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Cos;

    function Tan (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Tan_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Tan;

    function Cot (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Cot_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Cot;

    function Arcsin (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 or abs X > 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arcsin_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Arcsin;

    function Arccos (X, Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 or abs X > 1.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arccos_Cycle (Long_Float (X), Long_Float (Cycle)));
    end Arccos;

    function Arctan (Y : Float_Type'Base; X : Float_Type'Base := 1.0) return Float_Type'Base is
    begin
        if X = 0.0 and Y = 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arctan (Long_Float (Y), Long_Float (X)));
    end Arctan;

    function Arctan (Y : Float_Type'Base; X : Float_Type'Base := 1.0; Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 or (X = 0.0 and Y = 0.0) then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arctan_Cycle (Long_Float (Y), Long_Float (X), Long_Float (Cycle)));
    end Arctan;

    function Arccot (X : Float_Type'Base; Y : Float_Type'Base := 1.0) return Float_Type'Base is
    begin
        if X = 0.0 and Y = 0.0 then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arctan (Long_Float (Y), Long_Float (X)));
    end Arccot;

    function Arccot (X : Float_Type'Base; Y : Float_Type'Base := 1.0; Cycle : Float_Type'Base) return Float_Type'Base is
    begin
        if Cycle <= 0.0 or (X = 0.0 and Y = 0.0) then
            raise Ada.Numerics.Argument_Error;
        end if;
        return Float_Type'Base (C_Arctan_Cycle (Long_Float (Y), Long_Float (X), Long_Float (Cycle)));
    end Arccot;

end Ada.Numerics.Generic_Elementary_Functions;
