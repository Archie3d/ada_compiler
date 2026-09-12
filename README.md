# Ada compiler

This project implements an Ada 83/95 subset compiler in C++. It uses [QBE](https://c9x.me/compile/) as a backend.

> This is an experimental project built with a help of AI.

The compilation is performed in three stages:
- Ada to QBE compiler frontend, which translates Ada source code to QBE intermediate language (IR).
- QBE backend, which compiles IR to the target's assembly.
- Target's assemly and linker combines the QBE's output with the language runtime to produce an executable.

A compiler driver is provided. This executes all the steps of the compilation to go from Ada source code to an executable.

## Building

```shell
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
The build also compiles the vendored `qbe` submodule into `build/qbe/qbe`, and
the C run time into `build/runtime/libadart.a`.

### Building on Windows
When compiling on Windows use [Msys2](https://www.msys2.org/) environment.
You will need `gcc` (mingw), `make`, and `cmake` installed.

## Installing

```shell
cmake --install build --prefix /usr/local
```

which lays the compiler out the way GNAT lays itself out, the names saying what
each directory holds:

| Path | Contents |
| --- | --- |
| `bin/ada`, `bin/adac`, `bin/qbe` | The driver, the front end and the backend |
| `lib/ada/adainclude/` | The predefined environment as Ada source |
| `lib/ada/adalib/libadart.a` | The C run time every program is linked against |

An installed `ada` works out where it stands, following the symbolic link it
was reached through and searching `PATH` when it was named without a directory,
and reads the environment from `../lib/ada/adainclude` beside itself. So the
tree can be moved after installing, and nothing has to be pointed at by hand.

## Using the compiler

`ada` is the driver. It runs `adac` to produce QBE IL, `qbe` to translate that
IL into assembly and `cc` to assemble and link the program with the run time
library, removing the intermediate files afterwards.

```
ada [-o <program>] <source> [<source>...]
```

| Option | Effect |
| --- | --- |
| `-o <file>` | Name of the produced file |
| `--emit-ir` | Stop after generating QBE IL |
| `-S` | Stop after generating assembly |
| `-k` | Keep the intermediate files |
| `--no-stdlib` | Leave the predefined environment out of the unit search path |
| `-v` | Print each command before running it |

The driver tells `adac` where the predefined environment is, so nothing needs
naming on the command line. `--no-stdlib` withholds it, and a program that
withs a predefined unit is then rejected with `cannot find the unit`; the run
time library is linked either way, since a program raising `Constraint_Error`
calls into it whether or not it names a predefined unit.

The `ADAC`, `QBE`, `CC`, `ADA_RUNTIME` and `ADA_LIBRARY` environment variables
override what the driver picks up, and have the last word over anything it
works out for itself.

`adac` itself is a front end only: it reads Ada source files and writes QBE IL,
and never runs another program.

```
adac [options] <source> [<source>...]
```

| Option | Effect |
| --- | --- |
| `-o <file>` | Name of the produced file, `-` for standard output |
| `-I <dir>` | Another directory to look for units in |
| `--stdlib <dir>` | Where the predefined environment lives |
| `--no-stdlib` | Leave the predefined environment out altogether |

Only the main procedure needs to be named. Every unit a `with` clause mentions
is looked for on the library path and compiled along with it, so a package
specification and its body are found by their file names rather than listed.
Several sources may still be given at once.

## Layout

| Path | Contents |
| --- | --- |
| `adac/` | The compiler: lexer, parser, semantic analysis and QBE code generation |
| `ada/` | The driver that chains `adac`, `qbe` and `cc` |
| `runtime/ada/` | The predefined environment, written in Ada |
| `runtime/adart.c` | Run time support (`'Image`, `'Value`, real number formatting, array comparison, raising and reporting exceptions) |
| `runtime/adaio.c` | The file layer behind `Text_IO`, `Sequential_IO`, `Direct_IO` and `Stream_IO`; the two C files build into `libadart.a` |
| `tests/ada/` | Ada test programs with their expected output |
| `tests/golden/` | Recorded QBE IL used to notice code generation changes |
| `qbe/` | The QBE backend, as a submodule |

## Supported language

Objects, constants and named numbers; integer, floating point, Boolean,
character, enumeration, array, record, access and private types; discriminants
and variant records; subtypes with range and discriminant constraints; the full
expression syntax including `mod`, `rem`, `**`, `&` and
short circuit operators; `if`, `case`, `while`, `for`, plain loops with `exit`,
blocks; procedures and functions with `in`, `out` and `in out` parameters,
recursion and nested subprograms with up level references; package
specifications and bodies with elaboration code; the `'First`, `'Last`,
`'Length`, `'Range`, `'Pos`, `'Val`, `'Succ`, `'Pred`, `'Digits`, `'Width`,
`'Image`, `'Value`, `'Address`, `'Size`, `'Read`, `'Write`, `'Input` and
`'Output` attributes; a `'Size` representation clause; `pragma Import`; generic
packages and subprograms with their instantiations; run time range checks
raising `Constraint_Error`, `raise` statements and exception handlers.

Integer types declared with `type T is range L .. H` and their subtypes are
checked wherever a value crosses into them: on assignment, on initialisation,
on argument passing, on conversion and on a returned result.

`Integer` uses 32-bit signed storage and `Long_Integer` uses 64-bit signed
storage. User-defined integer ranges exceeding 32 bits also use 64 bits;
subtypes preserve their parent's storage width. Integer arithmetic checks
addition, subtraction, multiplication, division, negation, `abs`, and `**`
for machine-range overflow. Division, `rem`, and `mod` check zero divisors.
These failures raise `Constraint_Error`, as do out-of-range numeric conversions.
The checked operations currently call C runtime helpers, which adds call overhead.

`Long_Integer` is supported by `'Image`, `'Value`, generic `Integer_IO`, and
`for` loops. A loop tests its final value before incrementing or decrementing,
so a loop ending at a machine limit does not wrap around.

Calls with argument lists, bare function names, and package-selected names use
the expected result type when selecting an overload, and report ambiguity when
several profiles remain. A bare call may omit all arguments when every formal
has a default. Procedure call statements select procedures, and enumeration
literals use the expected enumeration type. Shared formal types provide context
for nested calls. Named arguments cannot be repeated or followed by positional
arguments. Full resolution of mutually overloaded nested expressions remains
future work.

Omitted default expressions are evaluated at each call, including imported
calls, with names bound in the declaration scope. Each parameter in a grouped
profile gets its own default evaluation. Explicit arguments skip their defaults;
default evaluation can raise an exception before the called body is entered.
Only `in` parameters may have defaults, and the default must have a compatible
type. Defaults use the same value conventions as explicit arguments.

Record and constrained-array function results use caller-provided storage.
Unconstrained-array results preserve their bounds and are copied into the caller
before use, including in nested calls, indexing, attributes, and constrained
object initialization or assignment. Length mismatches raise `Constraint_Error`;
null results preserve their bounds and have zero length. Functions of every
result type that fall through without returning raise `Program_Error`, including
when a handler finishes without returning a value.

Variable-size result temporaries remain on the caller's stack until it exits,
so repeated calls in long loops can accumulate stack storage. Inference of
unconstrained aggregate bounds remains future work; return a constrained local
object or use a constrained subtype for these aggregates.
The new internal return convention requires rebuilding Ada code and using the
matching runtime; imported C calls retain their existing convention.

Within block, subprogram, and package-body handlers, bare `raise;` re-raises the original
exception, even after a nested handler or called routine handles a different
exception. Bare raises outside handlers or inside an enclosed body are rejected,
following the [Ada raise-statement rules](https://docs.adacore.com/live/wave/arm22/html/arm22/RM-11-3.html).
An unhandled exception during library elaboration is reported with exit status 1
before the main procedure is called. Library package declarations and body
statements execute in declaration order within the loader's unit order, including
nested packages and library generic instances. Package-body handlers can recover
and let elaboration continue; declaration failures bypass that package's handlers,
and failures raised by a handler propagate outward.

Packages declared inside subprograms and their blocks elaborate whenever
execution reaches the declaration. This includes generic instances: each call
gets fresh local state, recursive calls keep independent state, and package
routines can access enclosing variables through static links. Package bodies
and handlers execute at that point, including when the enclosing routine is
called during library startup. Existing generic formal-parameter restrictions
still apply.

Dependency ordering and declarations inside library-level statement blocks
remain future work.

Local one-dimensional arrays can use runtime index constraints or take their
bounds from an initializer:

```ada
Buffer : String (1 .. N) := (others => ' ');
Copy   : String := Make_Text;
```

The object keeps its bounds for its lifetime. Assignment checks lengths and
slides the source to the target bounds; indexing checks runtime bounds. Bounds
also travel through slices, function results, calls, and captured variables.
`others` aggregates and component defaults are evaluated per element. Grouped
object declarations evaluate an initializer separately for each object.

Dynamic local array data uses heap storage owned by the enclosing call and is
released on normal return or exception propagation. Repeated block entries retain
allocations until that call exits. This implementation uses signed 32-bit bounds,
limits lengths to `Integer'Last`, checks allocation sizes, and raises
`Storage_Error` on allocation failure. Runtime named subtype constraints,
library-level dynamic objects, and positional/named runtime-bounded aggregates
remain unsupported.

Floating point types are declared with `digits`, optionally with a range:

```ada
type Coefficient is digits 10 range -1.0 .. 1.0;
type Real is digits 8;
subtype Probability is Real range 0.0 .. 1.0;
```

Up to six digits are held in a single precision number and up to fifteen in a
double precision one, which is what the `s` and `d` types of QBE provide; more
than fifteen digits is rejected. A range is checked at the same places as an
integer one. Following Ada, the two families of numbers stay apart: a whole
number literal is not a real value, so `C : Coefficient := 1;` is an error
while `C : Coefficient := 1.0;` is not, `mod` and `rem` need integer operands,
the exponent of `**` is always an integer, and converting a real value to an
integer rounds rather than truncates.

Strings are arrays of characters and carry their bounds along with the data, so
an unconstrained `String` parameter answers `'First`, `'Last` and `'Length` at
run time. Slices, `&` on strings, and comparison with `=`, `/=`, `<`, `<=`, `>`
and `>=` are supported; assigning a value of a different length raises
`Constraint_Error`.

### Composite comparisons

Array equality compares corresponding elements by position, independently of
lower bounds, and requires equal lengths. Null arrays compare equal even when
their bounds differ. Comparison is recursive for record and array elements;
floating elements use numeric equality rather than byte equality. Ordering is
available for one-dimensional arrays of discrete elements and is lexicographic.
Arrays of floating-point, record, array, or access elements support equality
but not predefined ordering.

Record equality compares common components and discriminants, then only the
active variant's components. Padding and inactive variant storage are ignored.
These rules follow [Ada 95 RM 4.5.2](https://www.adaic.org/resources/add_content/standards/95lrm/ARM_HTML/RM-4-5-2.html).

### Access types and allocators

An access type designates objects taken from storage rather than declared. A
type may be named before it is described, which is what lets a record and the
access type pointing at it be written in either order:

```ada
type Node;
type Link is access Node;

type Node is
   record
      Value : Integer;
      Next  : Link;
   end record;
```

`new Node` takes storage for one node and answers a `Link` to it; `new Node'(1,
null)` gives the new object its value at the same time, and `new String (1 ..
5)` says how long the array is to be. An allocator has no type of its own, so
what it makes is read from where it is used, the way `null` is. Storage comes
out cleared, so an access component of a new object starts as `null`, and a
`Storage_Error` is raised when the request cannot be met.

`P.all` is the object a link designates, and it may be left out in front of a
component or an index: `P.Value` and `P.all.Value` are the same thing, as are
`T (1)` and `T.all (1)`. Reaching through `null` raises `Constraint_Error`.

Storage is given back through `Ada.Unchecked_Deallocation`, a generic procedure
over the access type and what it designates:

```ada
procedure Dispose is new Ada.Unchecked_Deallocation (Node, Link);
...
Dispose (Head);    -- Head is null afterwards.
```

Nothing checks that no other access value still designates the object, which is
what "unchecked" says: reaching through one of those afterwards is the program's
own doing.

### Private types

A package may name a type without showing what it is made of. Outside the
package the name, the operations the package declares and assignment and
equality are all there is; the components, the aggregate and the predefined
operators are out of reach. A type declared `limited private` lends not even
assignment or equality.

```ada
package Piles is
   type Pile is private;
   Empty : constant Pile;
   procedure Push (P : in out Pile; Value : in Integer);
private
   type Contents is array (1 .. 8) of Integer;
   type Pile is
      record
         Items : Contents;
         Top   : Integer := 0;
      end record;
   Empty : constant Pile := (Items => (others => 0), Top => 0);
end Piles;
```

`Empty` above is a deferred constant: the visible part names it so that users
may write it, and the private part gives it a value once the representation is
known. A constant may be left without a value only there, and one never given
a value is an error, as is a private type never completed.

The full declaration fills in the very type the visible one named, so an access
type or a subprogram that mentioned it goes on meaning the same thing.

### Discriminants and variant records

A discriminant is a component named in the type declaration and fixed when an
object is declared. A variant part makes the rest of the components depend on
it:

```ada
type Figure is (Point, Circle, Rectangle);

type Shape (Kind : Figure) is
   record
      Label : Character;
      case Kind is
         when Point  => null;
         when Circle => Radius : Integer;
         when others => Width, Height : Integer;
      end case;
   end record;

subtype Round is Shape (Circle);

C : Round          := (Kind => Circle, Label => 'c', Radius => 5);
R : Shape (Rectangle) := (Rectangle, 'r', 3, 4);
```

The alternatives share their storage, so a `Shape` has one size whichever kind
it holds and an object of it never has to move. A discriminant is fixed when the
object is declared and never changes, which is why it takes no default and
cannot be assigned to; an object of the unconstrained type is rejected for
saying nothing about which variant it has.

Where nothing has fixed the discriminant beforehand, the aggregate settles it
itself, so `new Shape'(Rectangle, 'r', 3, 4)` makes a rectangle without the
subtype having to say so.

The choices of a variant part are read the same way as those of a case: static
values, ranges, `|` between them and `others` at the end, each value covered
exactly once and all of them covered. Where a subtype fixed the discriminant,
naming a component of another alternative is an error the compiler reports;
where nothing fixed it, as in a parameter of the unconstrained type, the value
carries the answer and reaching for the wrong component raises
`Constraint_Error`:

```ada
function Area (S : in Shape) return Integer is
begin
   case S.Kind is
      when Point     => return 0;
      when Circle    => return 3 * S.Radius * S.Radius;
      when others    => return S.Width * S.Height;
   end case;
end Area;
```

### Choosing between values

A case chooses on a discrete value. A choice is a single value, a range, a
subtype mark standing for the range it holds, or several of those separated by
`|`, and `others` takes whatever is left:

```ada
case Today is
   when Mon        => Compute_Initial_Balance;
   when Fri        => Compute_Closing_Balance;
   when Tue .. Thu => Generate_Report (Today);
   when Sat .. Sun => null;
end case;
```

Every choice has to be static, and between them the alternatives have to
account for every value the selector can take, each of them exactly once. The
seven days above are all covered, which is why that case needs no `others`;
leaving one out is an error rather than a case that quietly does nothing, a
value covered twice is an error, and so is a choice outside what the selector's
subtype holds. Ada leans on all of that, and so does this compiler: a case
compiles to a plain choice with no run time check behind it.

### Numbers in another base

A based literal writes its value in any base from 2 to 16, with an optional
fraction and an exponent that scales by a power of that base:

```ada
Mask    : constant Integer := 16#FF#;
Pattern : constant Integer := 2#1010_1010#;
Scaled  : constant Integer := 16#1#E4;
Half    : constant Real    := 16#F.8#;
Small   : constant Real    := 16#1.0#E-2;
```

`Integer_IO.Put` takes a `Base` alongside `Width` and writes the value back in
the same notation, so `Put (255, 0, 16)` prints `16#FF#`. `'Image` stays
decimal.

## The predefined environment

Apart from `Standard` and `System`, which the compiler builds because the
language is defined in terms of them, the predefined units are ordinary Ada
source under `runtime/ada/`. They are compiled with the program that draws on
them, and nothing in them is spelled differently from what a program of your
own may write.

| Unit | File |
| --- | --- |
| `Ada` | `ada.ads` |
| `Ada.IO_Exceptions` | `ada-io_exceptions.ads` |
| `Ada.Text_IO` | `ada-text_io.ads` |
| `Ada.Text_IO.Integer_IO` | `ada-text_io-integer_io.ads`, `.adb` |
| `Ada.Text_IO.Float_IO` | `ada-text_io-float_io.ads`, `.adb` |
| `Ada.Text_IO.Enumeration_IO` | `ada-text_io-enumeration_io.ads`, `.adb` |
| `Ada.Integer_Text_IO` | `ada-integer_text_io.ads` |
| `Ada.Float_Text_IO` | `ada-float_text_io.ads` |
| `Ada.Sequential_IO` | `ada-sequential_io.ads`, `.adb` |
| `Ada.Direct_IO` | `ada-direct_io.ads`, `.adb` |
| `Ada.Streams` | `ada-streams.ads` |
| `Ada.Streams.Stream_IO` | `ada-streams-stream_io.ads` |
| `Ada.Unchecked_Deallocation` | `ada-unchecked_deallocation.ads`, `.adb` |

A unit lives in the file its name gives, lowered with each dot turned into a
hyphen, so `Ada.Text_IO.Integer_IO` is `ada-text_io-integer_io`. The
specification is `.ads` and the body `.adb`. A unit is looked for beside the
source that asked for it, then in each `-I` directory, then along
`ADA_INCLUDE_PATH`, and last in the library that came with the compiler, which
is `runtime/ada` in the build tree and `lib/ada/adainclude` once installed.

What these units cannot say in Ada they say with `pragma Import`, which ties a
declaration to an entry point in the C run time:

```ada
procedure Put_Line (Item : in String);
pragma Import (C, Put_Line, "__ada_put_line");
```

The pragma applies to the declaration just given, so each overload names the
routine that carries it out. Anything a package can write for itself it writes
for itself: `Integer_IO.Get` tests the range and raises `Data_Error`,
`Enumeration_IO` is built on `Enum'Image` and `Enum'Value`, and `Sequential_IO`
and `Direct_IO` hand an element to the run time as `Item'Address` and
`Element_Type'Size`, which is why one entry point serves every instantiation.

`System` declares `Address`, the type `'Address` yields, and `Storage_Unit`,
the number of bits `'Size` counts in. A size clause fixes how wide a type is
laid out, which is how `Ada.Streams` says that a stream element is a byte:

```ada
type Stream_Element is range 0 .. 255;
for Stream_Element'Size use 8;
```

### Input and output

Numbers and enumeration values are written through the generic children of
`Ada.Text_IO`, one instance per type:

```ada
package Level_IO is new Ada.Text_IO.Integer_IO (Level);
package Real_IO is new Ada.Text_IO.Float_IO (Real);
package Day_IO is new Ada.Text_IO.Enumeration_IO (Day);
```

Each instance carries the defaults Ada derives from the type it was made with,
so `Integer_IO.Put` lays out a field of `Num'Width` and `Float_IO.Put` shows
`Num'Digits - 1` places after the point, both overridable per call through
`Width`, or `Fore`, `Aft` and `Exp`. `Enumeration_IO.Put` writes the literal in
upper case unless given `Lower_Case`, and `Get` reads one back, raising
`Data_Error` on a word the type has no literal for. `Ada.Integer_Text_IO` and
`Ada.Float_Text_IO` are the instances on `Integer` and `Float`, which is what
they are in Ada, so a value of another numeric type needs an instance of its
own rather than a conversion.

`'Image` on an enumeration value gives the literal in upper case.

`Text_IO` works on files as well as on the standard ones. `File_Type`,
`File_Mode`, `Create`, `Open`, `Close`, `Delete`, `Reset`, `Is_Open`, `Mode`,
`Name` and `Form` manage a file, and `Put`, `Put_Line`, `Get`, `Get_Line`,
`New_Line`, `Skip_Line`, `End_Of_File` and `End_Of_Line` come in both a plain
form and one that names a file. `Set_Input` and `Set_Output` redirect the plain
form, and `Standard_Input`, `Standard_Output`, `Standard_Error`,
`Current_Input` and `Current_Output` name the files it would otherwise use.
The eight exceptions of `Ada.IO_Exceptions` are raised by the run time and
caught by ordinary handlers.

```ada
Create (Data, Out_File, "readings.txt");
Put_Line (Data, "first line");
Close (Data);

Open (Data, In_File, "readings.txt");
while not End_Of_File (Data) loop
   Get_Line (Data, Line, Last);
   Put_Line (Line (1 .. Last));
end loop;
Close (Data);
```

`Ada.Sequential_IO` and `Ada.Direct_IO` are generic on their element type and
read and write whole objects. `Direct_IO` additionally addresses the file by
element with `Read`, `Write`, `Index`, `Set_Index` and `Size`.

```ada
package Reading_IO is new Ada.Sequential_IO (Reading);
package Slot_IO is new Ada.Direct_IO (Slot);
```

`Ada.Streams.Stream_IO` opens a file as a stream. `Stream (File)` yields a
`Stream_Access`, and the `'Write`, `'Read`, `'Output` and `'Input` attributes
move a value of any type across it. `'Output` and `'Input` additionally carry
the bounds of an array whose type does not fix them, which is what makes
`String'Input` give back the string that `String'Output` wrote. `Read`, `Write`,
`Index`, `Set_Index` and `Size` work on the file directly in terms of
`Stream_Element_Array`.

```ada
Create (Archive, Out_File, "state.dat");
Channel := Stream (Archive);
Integer'Write (Channel, 1234);
String'Output (Channel, "hello");
```

## Generics

A generic unit written in Ada is remembered as the tokens it was written with,
and each instantiation parses them again with the formals bound to their
actuals. Formal types and formal objects with defaults are supported, in
positional or named notation.

```ada
generic
   type Element is private;
   Capacity : Integer := 4;
package Stacks is
   procedure Push (Value : Element);
   function Pop return Element;
end Stacks;

package Number_Stack is new Stacks (Integer);
package Letter_Stack is new Stacks (Element => Character, Capacity => 2);
```

Each instance has state and code of its own. An instance may stand at library
level or inside a subprogram; either way its state is elaborated once, with the
library.

The input and output generics are instantiated the same way as any other, since
they are written in Ada like the rest of the predefined environment.
