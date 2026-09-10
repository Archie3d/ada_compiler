-- Floating point input and output for the predefined Float type, which Ada
-- gives a name of its own because it is the instance nearly every program
-- wants.

with Ada.Text_IO;
with Ada.Text_IO.Float_IO;

package Ada.Float_Text_IO is new Ada.Text_IO.Float_IO (Float);
