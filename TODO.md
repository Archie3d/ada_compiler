# Compiler implementation roadmap

This is an incremental roadmap for the existing Ada subset, not a claim of full
Ada conformance. Keep `qbe/` unchanged. Add a small Ada regression program for
each behavior, plus a rejection test where a legality rule is involved.

The order below prioritizes correctness in supported constructs before larger
language extensions. Items labelled **audit** need focused reproductions before
choosing a fix. Other implementation observations come from the current source;
the suggested tests are acceptance criteria, not tests already run.

## Baseline already implemented

Do not restart these features from scratch; extend their existing support.

- [x] 32-bit `Integer` and 64-bit `Long_Integer`, integer range checks, checked
  integer arithmetic, zero-divisor checks, narrowing conversions, and loop
  termination at integer limits.
- [x] Integer `'Image`, `'Value`, and generic integer I/O support for 64-bit values.
- [x] Expected-result filtering and ambiguity diagnostics for calls with argument
  lists; named-argument validation and hiding of outer matching profiles.
- [x] Runtime tests reject a nonzero exit status even when stdout matches.
- [x] Basic packages, nested subprograms, generic types/objects and instantiation,
  private types, access-to-object types, exception handlers, and file I/O.
- [x] One-dimensional arrays, strings, slices, records, and a restricted form of
  discriminants and variant records.

These checkmarks describe tested subsets, not completion of every related Ada rule.

## 1. Correctness and semantic completeness

### Fuller overload resolution

Primary code: `adac/Sema.cpp`, `adac/Scope.cpp`.

- [ ] Resolve mutually overloaded nested expressions using candidate sets and
  surrounding context. The current shared-formal-type heuristic is incomplete.
- [x] Apply result-context selection and ambiguity checks to parameterless
  functions, package-selected names, enumeration literals, and calls using only
  defaults. Procedure statements select procedures, not functions whose values
  would be discarded. Covered by `parameterless.adb` and `parameterlesserrors.adb`.
- [x] Evaluate omitted defaults for supported Ada and imported call forms at each
  call, using declaration-scope bindings. Preserve grouped parameter defaults,
  captured environments, subtype checks, and exception propagation. Covered by
  `defaultcalls.adb` and `defaulterrors.adb`; composite-valued defaults are also
  covered by `compositereturns.adb`.
- [ ] Add user-defined operator declarations and calls, including operator symbols
  such as `function "+" (...) return T`.
- [ ] **Audit** visibility, `use` clauses, homographs, duplicate declarations, and
  specification/body conformance. Validate parameter modes, names, defaults, and
  return profiles where applicable; do not defer missing bodies to linker errors.

Tests: an overloaded inner call resolved by an outer formal; ambiguous zero-argument
functions; qualified enumeration literals; a default expression with a side effect;
operator overloads; nested hiding and invalid specification/body pairs.

### Composite values and returns

Primary code: `adac/QbeEmitter.cpp`, `adac/Type.cpp`, `adac/Sema.cpp`.

- [x] Give array/record results caller-owned storage. Fixed-size results use a
  hidden destination pointer. Unconstrained array results transfer a heap copy
  plus bounds; the caller immediately copies to its stack and frees the transfer
  buffer. Exceptions skip unfinished results. Covered by `compositereturns.adb`.
- [x] Preserve bounds of unconstrained array results through constrained-object
  initialization, assignment with sliding/length checks, indexing, attributes,
  and nested calls, including null ranges. Covered by `compositereturns.adb`.
- [ ] Reclaim variable-size expression temporaries before the caller exits;
  repeated calls in loops currently accumulate caller stack storage. Include
  normal and exceptional exits in the dynamic-array lifetime work.
- [ ] Infer array aggregate bounds in unconstrained contexts. Until implemented,
  use a constrained subtype or local object when returning an array aggregate;
  unsupported direct returns are diagnosed (`returnerrors.adb`).
- [x] Compare non-character arrays element by element, recursively for composite
  elements. Discrete-element array ordering is lexicographic; arrays of other
  element types support equality only. Covered by `arraycompare.adb` and
  `comparisonerrors.adb`, including runtime null ranges and non-1 lower bounds.
- [x] Compare only the active components of variant records, after checking common
  fields and discriminants. Covered by `recordcompare.adb`, including deterministic
  differences in inactive storage and nested variant records.
- [ ] Enforce array type identity independently of element-type equality;
  `typesCompatible` currently accepts distinct array types with the same element
  root type. Preserve contextual typing of string literals and aggregates.
- [ ] **Audit** aggregate completeness, duplicate choices, record defaults, array
  sliding, overlapping slice assignments, and component subtype checks.
- [ ] Preserve constraints and defaults for every name in a grouped component
  declaration. The parser currently moves the full subtype/default expression
  only to the last field in declarations such as `X, Y : Integer := 3`.

Tests: a function returning a local record; two live string results from different
calls; integer arrays differing beyond their first bytes; equal variants with
irrelevant storage differences; distinct array types; grouped field defaults;
self-overlapping slice assignment.

### Calls, exceptions, and elaboration

Primary code: `adac/QbeEmitter.cpp`, `adac/Sema.cpp`, `adac/UnitLoader.cpp`.

- [ ] Implement scalar `out`/`in out` copy-in/copy-out behavior and checks on the
  actual object's subtype. The current implementation passes all writable
  parameters by reference. Treat composite parameter mechanisms separately.
- [ ] Diagnose invalid function return usage and handle reaching a function's end
  without a result. Composite functions now raise `Program_Error` on fallthrough;
  scalar functions still receive an implicit zero return value.
- [ ] Implement bare `raise;` as re-raising the active exception, including after
  a nested handler. The current emitter substitutes `Constraint_Error` when no
  exception symbol is present.
- [ ] Retain exception occurrence bindings (`when E : ...`) instead of discarding
  them, then add occurrence information and messages.
- [ ] **Audit** handler choice legality and propagation from declarations, package
  bodies, called routines, and handlers themselves.
- [ ] Preserve elaboration order between declarations and package body statements.
  The emitter currently runs all global initializers before package statements.
- [ ] Stop before calling the main procedure when library elaboration fails.
- [ ] Elaborate local generic package instances per execution of their enclosing
  scope, with local state and captures. They currently become library globals.
- [ ] **Audit** unit visibility, dependency cycles, explicit-source/spec/body
  loading, and elaboration-before-use checks.

Tests: a constrained actual passed to an unconstrained scalar formal; a function
falling through; re-raising a user exception; a library initializer that raises;
initializers depending on a package body; two calls to a procedure containing a
local generic instance; mutually dependent unit specifications/bodies.

### Numeric, bounds, and representation follow-up

- [ ] **Audit** remaining attributes (`'Pos`, `'Val`, `'Succ`, `'Pred`, `'First`,
  `'Last`, `'Range`) for base/subtype rules, operand types, arity, and width.
- [ ] Support the full signed literal boundary, including the most-negative
  64-bit decimal literal, without overflowing the lexer or static evaluator.
- [ ] Separate universal/static arithmetic from machine arithmetic where needed;
  diagnose invalid static expressions without host overflow or premature narrowing.
- [ ] Complete real arithmetic edge cases, including negative exponents and
  zero divisors; define the supported floating-point model and its checks.
- [ ] **Audit** size calculations, array bounds/offset arithmetic, allocation
  overflow, and 64-bit indices. Array descriptors and several offset paths still
  use 32-bit bounds even though scalar `Long_Integer` now works.
- [ ] Validate `'Size` against supported representations and the represented range;
  add alignment, enumeration representation, and record representation clauses
  in separate steps. Reject unsupported clauses/conventions clearly.
- [ ] Give ignored pragmas an explicit diagnostic policy. Required semantics must
  not silently disappear; distinguish unsupported standard pragmas from optional
  implementation-defined ones.

Tests: literal endpoints, invalid attribute arguments, `2.0 ** (-3)`, huge array
sizes, signed small representations, invalid size clauses, and unsupported imports.

## 2. Arrays and records

### Dynamic arrays and constraints

- [ ] Runtime scalar subtype bounds and array index constraints, such as
  `Buffer : String (1 .. N)` where `N` is a parameter.
- [ ] A consistent descriptor for data address, bounds, lengths, and element
  strides; use it in calls, slices, results, assignment, and attributes.
- [ ] Runtime storage allocation/reclamation for local arrays and temporaries,
  including null ranges and checked size computations.
- [ ] General one-dimensional array concatenation, including element/array
  combinations. Concatenation is currently specialized to character arrays.

Tests: varying lengths across calls, null arrays, non-1 lower bounds, descriptor
passing through nested calls, length mismatch, and concatenating integer arrays.

### Multidimensional arrays

- [ ] Multiple index types and constraints in the semantic type representation.
  Parsing accepts an index list, but semantic analysis explicitly rejects it.
- [ ] Per-dimension bounds, strides, indexing checks, and dimension arguments to
  attributes such as `Matrix'Length (2)`.
- [ ] Multidimensional aggregates, assignment, equality, and parameter/result
  conventions, building on the descriptor and composite-return work above.

Tests: constrained and unconstrained matrices, different lower bounds per axis,
empty dimensions, and out-of-range indices in each dimension.

### Discriminated records

- [ ] Default discriminants and the distinction between constrained and
  unconstrained objects; support assignments that legally change discriminants.
- [ ] Runtime discriminant constraints and components whose bounds depend on them.
- [ ] Nested variant parts, with layout, initialization, selection checks, and
  equality respecting every active variant level.

Tests: defaulted discriminants, a record containing `String (1 .. Length)`, variant
changes on an unconstrained object, and nested alternatives.

## 3. Additional types, access values, and declarations

- [ ] Modular integer types: modulus, wraparound arithmetic, logical operations,
  conversions, and boundary behavior. Keep these distinct from checked signed integers.
- [ ] Ordinary fixed-point types (`delta` and range), followed separately by decimal
  fixed-point types (`delta` and `digits`), with scaling, rounding, and checks.
- [ ] Complete derived-type behavior, including inherited primitive operations and
  explicit conversions, rather than merely copying representation metadata.
- [ ] General access types, `aliased` objects, `'Access`, access-to-constant, and
  accessibility checks. Existing named access types and allocators are a starting point.
- [ ] Access-to-subprogram types and indirect calls. Nested subprogram values need
  both a code pointer and the appropriate environment/static link lifetime.
- [ ] Object, package, and subprogram renaming. Exception renaming already exists.
- [ ] Separate subunit loading and parent-scope analysis. An `is separate` stub is
  parsed today, but that is not an implementation of separate bodies.
- [ ] Labels and `goto`, with legality checks for transfers across scopes.

Tests: modular wraparound, fixed-point rounding, derived operations, access to a
local object escaping its lifetime, callback invocation, renaming identity, and
subunits referring to enclosing declarations.

## 4. Generic completeness

Primary code: `Parser::parseGenericDeclaration`, `Sema::bindGenericFormals`, and
`Sema::analyzeGenericInstantiation`.

- [ ] Parse and enforce formal type categories fully: private/limited private,
  derived, array, access, modular, and fixed-point formals as their types become
  available. The current parser skips much of each formal type definition.
- [ ] Formal subprograms, operator actuals, and default actuals (`<>`).
- [ ] Formal packages and matching of their generic contracts.
- [ ] Nonstatic formal objects and their modes; current object actuals must fold
  to static values. Evaluate actual expressions at instantiation elaboration.
- [ ] Qualified type actuals, duplicate/named-argument validation, and default
  expressions that refer to earlier formals.
- [ ] Check generic bodies against their declared contracts, not solely against
  the concrete types available after token-based instantiation.
- [ ] Remove library-specific restrictions from general generic matching; for
  example, discrete formal matching currently rejects `Character` with an I/O-
  specific diagnostic.

Tests: generic sorting with a formal comparison function, a formal array type,
a formal package, a dynamic capacity actual, and an illegal generic body that
happens to work for one instantiation.

## 5. Tagged records and object-oriented features

This expands the original “Tagged records” item; a tag field alone is insufficient.

- [ ] Tagged type declarations, primitive operations, and record extensions.
- [ ] Inheritance and overriding, with profile and visibility checks.
- [ ] Class-wide types, tag checks, conversions, and dispatch tables/calls.
- [ ] Abstract types and operations; interfaces as a later extension.
- [ ] Controlled types and initialization/adjustment/finalization. First establish
  cleanup on normal return, scope exit, exceptions, and deallocation.

Tests: inherited operations, overridden dispatch through a class-wide value,
invalid downcasts, abstract-operation rejection, and cleanup during exception propagation.

## 6. Larger runtime and library extensions

These are later projects with substantial runtime requirements.

- [ ] Task types/objects, activation, entries, rendezvous, and termination.
- [ ] Protected objects, protected procedures/functions/entries, and synchronization.
- [ ] Delay/select/abort semantics and task-local exception state. The current
  exception globals and rotating image buffers are not a concurrent runtime.
- [ ] Complete streaming by type/components, including bounds and discriminants,
  user-defined stream operations, and a real stream abstraction. Current stream
  support largely transfers object bytes rather than implementing all type semantics.
- [ ] Extend numeric text input (`'Value` and `Integer_IO`) with based literals,
  underscores, exponents, and malformed-input checks; add floating `'Value`.
- [ ] Fill remaining I/O API gaps and add library packages incrementally, for
  example `Ada.Exceptions`, string handling, numerics, and calendar/time support.
- [ ] Wide characters, wide strings, and a documented source-encoding policy.

## 7. Development and validation infrastructure

- [ ] Choose and document the intended language-version baseline and optional
  later-version features. Keep a support matrix that distinguishes parsed,
  semantically checked, and correctly emitted constructs.
- [ ] For every implemented item, add focused execution and diagnostic tests;
  include boundary cases and interactions with existing features.
- [ ] Add crash, timeout, malformed-source, and sanitizer coverage; ensure both
  compiler subprocesses and generated programs have bounded test execution.
- [ ] Optionally compare shared supported programs with GNAT and introduce
  relevant conformance tests. Account for implementation-defined differences.
- [ ] Validate supported host/target combinations, ABI widths, installation and
  relocation, library lookup, and paths containing spaces. Track portability
  separately from language support.
- [ ] Consider a small lowering layer between semantic analysis and QBE emission
  when descriptors, cleanup, and dispatch make direct AST emission cumbersome.
  This is an architectural option, not a prerequisite for every small fix.
- [ ] Optimize checked arithmetic only after preserving its failure behavior in
  tests; the current runtime helpers provide a correctness baseline.

Suggested next sequence: exception/elaboration corrections; dynamic array
descriptors; multidimensional arrays. Modular types can be developed as a separate bounded extension after the numeric follow-up checks.
