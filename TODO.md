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

Primary code: `adac/sema/SemaCalls.cpp`, `adac/sema/SemaNames.cpp`,
`adac/sema/SemaDecl.cpp`, `adac/Scope.cpp`.

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

Primary code: `adac/QbeEmitter.cpp`, `adac/Type.cpp`,
`adac/sema/SemaTypes.cpp`, `adac/sema/SemaAggregates.cpp`.

- [x] Give array/record results caller-owned storage. Fixed-size results use a
  hidden destination pointer. Unconstrained array results transfer a heap copy
  plus bounds; the caller adopts the transfer buffer into its temporary
  allocation list. Exceptions skip unfinished results. Covered by `compositereturns.adb`.
- [x] Preserve bounds of unconstrained array results through constrained-object
  initialization, assignment with sliding/length checks, indexing, attributes,
  and nested calls, including null ranges. Covered by `compositereturns.adb`.
- [x] Reclaim variable-size array results and concatenation temporaries at
  statement boundaries, declaration-list completion, aggregate fill iterations,
  and while-condition evaluation. Exception handlers rewind abandoned storage;
  function exits release remaining allocations. Covered by `arraylifetimes.adb`
  and the instrumented `runtime.array_storage` allocation/free test.
- [x] Infer array aggregate bounds in unconstrained contexts: positional values
  start at the index subtype's lower bound; named choices supply their bounds.
  Includes local initialization, arguments, returns, single dynamic choices,
  and null ranges (`inferredaggregates.adb`, `inferredaggregatechecks.adb`).
  `others` without contextual bounds remains illegal (`returnerrors.adb`).
- [x] Compare non-character arrays element by element, recursively for composite
  elements. Discrete-element array ordering is lexicographic; arrays of other
  element types support equality only. Covered by `arraycompare.adb` and
  `comparisonerrors.adb`, including runtime null ranges and non-1 lower bounds.
- [x] Compare only the active components of variant records, after checking common
  fields and discriminants. Covered by `recordcompare.adb`, including deterministic
  differences in inactive storage and nested variant records.
- [x] Enforce array type identity independently of element-type equality.
  Subtypes and slices retain their declared array identity; distinct declarations
  are incompatible in assignments, calls, returns, comparisons, qualifications,
  and initializers. String literals, character concatenations, and aggregates
  use contextual types. Covered by `arrayidentity.adb`, `arrayidentityerrors.adb`,
  and `arrayliteralambiguity.adb`, including overload selection and ambiguity.
- [ ] Audit explicit array conversions for static component-subtype matching,
  index conversions, bound sliding, view conversions, and runtime checks.
  The current supported cross-type conversion requires identical component
  subtype objects and compatible index types (or two integer index types).
- [ ] **Audit** aggregate completeness, duplicate choices, record defaults, array
  sliding, overlapping slice assignments, and component subtype checks.
- [x] Preserve constraints and defaults for every name in a grouped component
  declaration, including variant alternatives. Each field owns its complete
  subtype/default syntax, and each object evaluates defaults separately for
  its active fields. Inactive variant defaults are skipped. Covered by
  `groupedfields.adb`, `groupedfieldchecks.adb`, and `groupedfielderrors.adb`:
  scalar ranges, constrained strings/matrices, nested discriminant constraints,
  default side effects, exception propagation, and invalid defaults.

Tests: a function returning a local record; two live string results from different
calls; integer arrays differing beyond their first bytes; equal variants with
irrelevant storage differences; distinct array types; grouped field defaults;
self-overlapping slice assignment.

### Calls, exceptions, and elaboration

Primary code: `adac/emitter/QbeCalls.cpp`, `adac/emitter/QbeFunctions.cpp`,
`adac/emitter/QbeStatements.cpp`, `adac/sema/SemaCalls.cpp`,
`adac/sema/SemaDecl.cpp`, `adac/sema/SemaPackages.cpp`, `adac/UnitLoader.cpp`.

- [x] Implement Ada scalar `out`/`in out` copy-in/copy-out behavior. Each formal
  has separate storage, initialized according to its mode. Normal return checks
  the actual object's subtype before copy-back; propagated exceptions skip it.
  Copy-back uses formal declaration order, and actual addresses are evaluated
  once. Imported C conventions and composite parameter mechanisms stay separate.
  Covered by `scalarcopy.adb`, `scalarcopychecks.adb`, and `scalarcopyerrors.adb`:
  static/runtime subtypes, failed copy-in/copy-back, nested calls and captures,
  aliased actuals, handlers, exceptional/early returns, component actuals,
  recursion, 64-bit integers, enumeration, real and access values.
- [x] Diagnose value returns from procedures, bare returns from functions, and
  returns outside subprograms. Every result representation now raises
  `Program_Error` on fallthrough, including after a handled exception. Covered
  by `functionfallthrough.adb`, `exceptionusageerrors.adb`, and `compositereturns.adb`.
- [x] Implement bare `raise;` in block/subprogram handlers by preserving the
  handled exception's identity and name, including across nested handlers and
  calls. Reject bare raises outside handlers or inside bodies enclosed by a
  handler. Covered by `reraise.adb`, `unhandledreraise.adb`, and
  `exceptionusageerrors.adb`.
- [ ] Retain exception occurrence bindings (`when E : ...`) instead of discarding
  them, then add occurrence information and messages.
- [ ] **Audit** handler choice legality and propagation from declarations, package
  bodies, called routines, and handlers themselves. Subprogram declaration and
  handler propagation have regression coverage in `reraise.adb`.
- [x] Emit library/nested package-body handlers, including recovery and re-raise.
  Declaration failures bypass that package's handlers; failures within a handler
  propagate outward. Covered by `packagehandlers.adb`,
  `packagedeclarationfailure.adb`, and `packagehandlerfailure.adb`.
- [x] Preserve declaration/body execution order within the loader's unit order,
  including public/private parts, nested packages, component defaults, and
  library generic instances. Covered by `elaborationorder.adb` and
  `genericelaborationorder.adb`. Dependency ordering remains a separate audit.
- [x] Stop before calling the main procedure when library elaboration fails;
  report the exception and exit with status 1. Covered by `elaborationfailure.adb`
  and `elaborationbodyfailure.adb` (stdout, stderr, and exit status).
- [x] Elaborate ordinary packages and generic package instances declared inside
  subprograms and their blocks when execution reaches the declaration. Objects
  belong to the enclosing activation; package routines use its static link.
  Covered by `localpackages.adb`, `localgenericstate.adb`,
  `localpackagestartup.adb`, and `localpackagefailure.adb`: repeated calls, block
  re-entry, independent instances, recursion, captures, startup calls, recovery,
  and failure propagation.
- [ ] Support declarations inside library-level statement blocks, including
  object storage, nested subprogram emission, and package elaboration.
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

- [x] Runtime one-dimensional constraints on local objects, such as
  `Buffer : String (1 .. N)`, and initializer-constrained local objects such as
  `Text : String := Make_Text`. Includes null ranges, `others` aggregates,
  component defaults, grouped initialization, assignment, and indexing checks.
  Covered by `dynamicarrays.adb`, `dynamicarrayvalues.adb`, and
  `dynamicarrayfailures.adb`.
- [x] Local arrays carry a data pointer and signed 32-bit bounds, matching
  unconstrained parameters and captured frame slots. Length is derived and
  element stride is static; descriptors survive calls, slices, results, and
  attributes. Captured reference parameters now load their pointer in their
  owning function as well as nested functions.
- [x] Allocate local array data with checked sizes and release all allocations
  on every enclosing function exit, including exception propagation. Reject
  lengths beyond `Integer'Last` with `Storage_Error`.
- [x] Reclaim local arrays at block exit, including labelled loop exits and
  exception propagation, while preserving enclosing objects and a block's
  locals during its own handler. Local arrays and temporaries use separate
  checkpointed allocation lists (`arraylifetimes.adb`).
- [x] Local named runtime discrete scalar subtype bounds, including `Integer`,
  `Long_Integer`, and enumeration subtypes. Save checked bounds once per
  elaboration; preserve them through aliases, nested routines/packages,
  recursion, and block re-entry. Apply them to initialization, assignment,
  conversions/qualification, input parameters/defaults, results, attributes
  (`'First`/`'Last`/`'Range`), membership, loops, and array index subtypes.
  Array aggregate inference uses the saved index lower bound. Non-null ranges
  check compatibility with the parent subtype; null ranges are allowed.
  Covered by `runtimescalars.adb` and `runtimescalarchecks.adb`; static-context,
  type, and unsupported-context diagnostics are in `runtimescalarerrors.adb`
  and `runtimescalarlibraryerrors.ads`.
- [ ] Extend runtime scalar constraints to anonymous subtype indications,
  library declarations, real subtypes, `'Width`, and streaming. These cases
  remain diagnosed. Scalar `out`/`in out` copy-back now checks saved runtime
  bounds, as listed under Calls, exceptions, and elaboration.
- [ ] Library-level dynamic arrays/types. Unsupported cases have diagnostics in
  `runtimearraylibraryerrors.ads`; local named array constraints are implemented
  below.
- [x] Positional/named aggregates with runtime target bounds, including a final
  `others`, static choice lists/ranges, and a single dynamic choice/range.
  Check lengths and choices; slide named aggregates without `others`; evaluate
  components into temporary storage before replacing the target. Dynamic choices
  also work with static target constraints. Covered by `runtimeaggregates.adb`,
  `runtimeaggregatechecks.adb`, and `dynamicaggregateerrors.adb`.
- [x] Infer bounds from unconstrained aggregates, including returned aggregates
  and initializer-constrained objects. Bounds are evaluated once; invalid index
  constraints precede component evaluation, and null ranges skip components.
  Covered by `inferredaggregates.adb` and `inferredaggregatechecks.adb`.
- [ ] Generalize descriptors to wider indices and per-dimension strides before
  extending the remaining array operations.
- [ ] General one-dimensional array concatenation, including element/array
  combinations. Concatenation is currently specialized to character arrays.

Tests: varying lengths across calls, null arrays, non-1 lower bounds, descriptor
passing through nested calls, length mismatch, and concatenating integer arrays.

### Multidimensional arrays

- [x] Statically constrained multidimensional types, represented internally as
  nested rows with separate index types and bounds. Storage is row-major.
- [x] Checked indexing on every axis and static dimension arguments to `First`,
  `Last`, `Length`, and `Range`. Index bounds currently must fit 32 bits.
- [x] Nested aggregates, assignment, equality, and parameters/results for fixed
  shapes, including null dimensions, enum indices, and record components.
  Covered by `matrices.adb`, `matrixshapes.adb`, and `matrixerrors.adb`.
- [x] Unconstrained multidimensional types, static subtype constraints, and
  runtime constraints on local objects. Carry per-dimension bounds through
  captured objects, parameters, and results; compute checked row strides.
- [x] Shape checks and bound sliding for assignment and constrained parameters
  and results, shape-aware equality, and contextual runtime aggregates.
  Initializer-constrained objects inherit all bounds from existing array values.
  Covered by `runtimematrices.adb`, `runtimematrixchecks.adb`, and
  `runtimematrixerrors.adb`.
- [x] Infer multidimensional aggregate bounds without an explicit target shape,
  including aggregates passed to unconstrained formals and returned directly.
  Prepare index choices once before component evaluation; require identical
  corresponding subaggregate bounds, including null ranges. Support positional,
  named, and string-literal rows. Covered by `inferredmatrices.adb` and
  `inferredmatrixchecks.adb`; reject unbounded `others` and non-subaggregate rows
  in `inferredmatrixerrors.adb` and `matrixaggregateerrors.adb`.
- [x] Local runtime array subtype declarations and runtime bounds in array type
  declarations, for one or multiple dimensions. Save checked bounds once per
  elaboration in the owning activation; preserve them through aliases, nested
  routines/packages, recursion, block re-entry, attributes, aggregates, objects,
  and sliding/checks for parameters and returns. Covered by `runtimearraytypes.adb`
  and `runtimearraytypechecks.adb`, with legality/unsupported-context diagnostics
  in `runtimearraytypeerrors.adb` and `runtimearraylibraryerrors.ads`.
- [ ] Runtime-constrained array components, allocators, `'Size`, and streaming.
  These uses remain diagnosed rather than using an incorrect static layout.
- [ ] Stream attributes for unconstrained multidimensional arrays.
- [ ] Wider descriptor indices and lengths; runtime lengths currently cannot
  exceed `Integer'Last`.

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

Scalar `out`/`in out` copy-in/copy-out and runtime discrete subtype bounds are
now implemented, along with array type identity checks and grouped record
component constraints/defaults. Suggested next step: audit overlapping slice
assignments and array sliding with focused reproductions (listed under Composite
values and returns). Library-level dynamic
arrays, runtime-constrained components, and wider descriptor indices/lengths
remain later array follow-ups.
Modular types can be developed as a separate bounded extension after the numeric
follow-up checks.
