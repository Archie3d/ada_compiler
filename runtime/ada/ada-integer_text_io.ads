-- Integer input and output for the predefined Integer type, which Ada gives a
-- name of its own because it is the instance nearly every program wants.

with Ada.Text_IO;
with Ada.Text_IO.Integer_IO;

package Ada.Integer_Text_IO is new Ada.Text_IO.Integer_IO (Integer);
