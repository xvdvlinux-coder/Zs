# Zs Programming Language

A systems programming language targeting Linux ARM64 (AArch64) architectures, focused on explicit pointer bounds over stack anchors (TAD) and dual return error handling via the CPU carry flag.

> [!WARNING]
> **Status: Experimental / Pre-1.0 (Alpha)**
> Zs is currently an experimental systems programming language under active development. It targets **Linux ARM64 (AArch64, ELF64, AAPCS64)** exclusively. It does not run natively on macOS (the Apple Silicon Darwin ABI is not supported). It is not ready for production environments. Compiler diagnostic and error messages are emitted in Spanish.
>
> **Important Safety Notice:** Current bounds checking covers **memory reads only**. Memory writes through pointers or indexed anchors are currently unchecked at runtime (see [L02](#l02-out-of-bounds-memory-writes-are-unchecked-silent)). Refer to [Known Limits and Caveats](#8-known-limits-and-caveats) before writing software in Zs.

> [!NOTE]
> **Design Specification vs. Current Implementation:**
> The document `zs_language_specification_and_plan.md` represents the original architectural design and **does NOT describe the current implementation**. The implementation in this repository differs from the specification in several critical aspects:
> 1. **Write safety:** The specification describes full spatial memory safety; the current compiler only bounds-checks memory reads. Memory writes (`*p = val`, `a[i] = val`) are unchecked (see [L02](#l02-out-of-bounds-memory-writes-are-unchecked-silent)).
> 2. **Locator representation:** The specification describes locators as single-word 64-bit pointers; the code generator implements `loc<T>` as a 24-byte fat pointer triplet `(ptr, base, limit)`.
> 3. **Hardware & fixed addressing:** There is no supported way to declare anchors at fixed memory addresses or volatile accesses (see [L13](#l13-fixed-address-anchors-and-volatile-accesses-not-supported-loud)).
> 4. **Slicing and views:** The specification describes dynamic slicing expressions and `.view()`; the compiler only evaluates integer literals in slicing (see [L15](#l15-dynamic-slice-bounds-unsupported-silent)) and rejects `.view()` as a syntax error (see [L09](#l09-method-call-view-fails-to-parse-loud)).
> 5. **Control flow & data structures:** Statements such as `break`, `continue` (see [L16](#l16-break-and-continue-ignored-in-codegen-silent)), and `branch` (see [L17](#l17-branch-statement-body-omitted-in-codegen-silent)), as well as `walk .. by` steps (see [L18](#l18-walk--by-k-step-clause-ignored-in-codegen-silent)) and `record` field access (see [L19](#l19-record-instantiation-and-field-access-limitations-silently-void--loud)), are parsed in front-end tests but are omitted or non-functional in machine code generation.

---

## Table of Contents

1. [Honest Description & Status](#1-honest-description--status)
2. [Philosophy](#2-philosophy)
3. [Who Zs Is For (and Who It Is Not For)](#3-who-zs-is-for-and-who-it-is-not-for)
4. [Quick Start](#4-quick-start)
5. [Language Tour](#5-language-tour)
6. [The Pointer and Memory Model (TAD)](#6-the-pointer-and-memory-model-tad)
7. [Verified Vocabulary and the 31–60 Method](#7-verified-vocabulary-and-the-3160-method)
8. [Known Limits and Caveats](#8-known-limits-and-caveats)
9. [Backward Compatibility Policy](#9-backward-compatibility-policy)
10. [Planned Fixes and Future Directions](#10-planned-fixes-and-future-directions)
11. [Reporting Issues and Verification Findings](#11-reporting-issues-and-verification-findings)
12. [Repository Layout and License](#12-repository-layout-and-license)

---

## 1. Honest Description & Status

Zs is a compiled systems programming language designed from the machine model up for the **ARM64 (AArch64)** architecture running Linux (ELF64 ABI). The compiler (`zsc`) is written in C99 and translates `.Zs` source files directly to GNU ARM64 assembly (`.s`), relocatable object files (`.o`), and native executables.

The compiler performs:
- Lexical analysis with memory-backed arena allocations.
- Recursive descent and Pratt precedence AST parsing.
- Semantic analysis, type checking, and symbol table resolution.
- Direct code generation of AArch64 assembly emitting instructions compliant with the AAPCS64 standard.

### Target Architecture & Environment
- **Instruction Set:** ARMv8-A (AArch64 / ELF64).
- **Operating System:** Linux (Direct syscall interface: `sys_write` 64, `sys_read` 63, `sys_openat` 56, `sys_close` 57, `sys_exit_group` 94).
- **Verified Host Platforms:** Native Linux ARM64 systems and Android Termux (AArch64).
- **Unsupported Host Platforms:** macOS (Darwin ABI) and x86_64 are **not supported**.
- **Compiler Language:** Compiler errors and diagnostic messages are emitted in Spanish (e.g., `error semántico: ...`, `Error de tipos: ...`).

---

## 2. Philosophy

The design of Zs is guided by the author's core principles:

- **Demystifying pointers:** Zs is an experience designed to take away the fear of pointers for a beginner or junior programmer who is just starting out. That fear decreases as one studies and works directly with Zs.
- **An imperfect tutor:** Zs is not a perfect tutor (no language or tool is), but it provides a practical foundation before transitioning to Zig or C/C++. That is its primary purpose: learning pointer arithmetic hands-on.
- **No obligation:** Nobody is obligated to adopt Zs as an industrial standard. If its bugs or limitations are bothersome, one can use Zig or C/C++ without any issue.
- **Different goals from Rust:** Zs does not compete with Rust, nor does it offer the comfort or compile-time guarantees of Rust's borrow checker: they are distinct models with different objectives. Zs specifically explores spatial safety over explicit stack anchors; it does not attempt to cover what Rust covers.
- **Creator responsibility:** An invisible error in pointer management is the responsibility of the language creator, not of the person learning or programming in Zs.
- **Statement from the author:**
  > *"Please create your own test folder and experiment: there are things that even I do not know. Being a creator does not mean knowing every corner. If you discover something I did not know, please let me know."*
- **Conservative evolution:** Zs prioritizes backward compatibility, maintainability, and predictable scalability over novelty or perfection.

---

## 3. Who Zs Is For (and Who It Is Not For)

### Who Zs Is For
- **Beginner and junior developers** who want to learn pointer arithmetic, spatial layout, and register-level ABIs without encountering silent undefined behavior on reads.
- **Low-level systems developers** writing experimental userspace programs on Linux ARM64 platforms.
- **Developers targeting standalone Linux environments** requiring direct Linux syscall execution without linking standard C libraries (`libc`). Note that `--freestanding` produces standalone Linux userspace binaries via `sys_exit_group` (syscall 94); it is **not bare-metal**, and there is no supported way to declare anchors at fixed memory addresses or volatile accesses.
- **Programmers seeking a predictable machine model** where failure is communicated through hardware CPU flags rather than exceptions or heap allocations.

### Who Zs Is Not For
- **Bare-metal or kernel development:** Zs targets userspace Linux ARM64. There is no supported way to declare anchors at fixed memory addresses or volatile accesses, nor does Zs provide bare-metal hardware initialization.
- **Developers seeking the compile-time safety of Rust:** Zs does not feature a borrow checker, lifetime annotations, or compile-time thread-safety proofs.
- **Production industrial applications:** Zs is in an experimental pre-1.0 state with known backend limits.
- **Cross-platform multi-target projects:** Zs only supports Linux ARM64 at this time.
- **Applications requiring memory write verification:** While Zs bounds-checks memory reads through pointers and array indices, **writes through pointers and indices are currently unchecked at runtime** (see [L02](#l02-out-of-bounds-memory-writes-are-unchecked-silent)).
- **Applications requiring concurrent threading:** Zs has no built-in concurrency primitives or data race detectors.

---

## 4. Quick Start

### 4.1 Prerequisites
To build and use Zs, your system must run Linux on ARM64 (`aarch64`) with:
- A C compiler supporting C99: `gcc` or `clang`
- GNU Assembler (`as`) and GNU Linker (`ld`) from `binutils`
- GNU `make`

On Debian/Ubuntu (AArch64):
```bash
sudo apt-get update
sudo apt-get install -y build-essential
```

On Android Termux (AArch64):
<!-- TODO(author): confirm Termux packages -->

### 4.2 Building the Compiler
Clone the repository and compile the driver and test suites using `make`:
```bash
make
```
This builds the native compiler binary at `build/zsc`.

To execute the test suite:
```bash
make test-all
```
All 60 tests across the 6 verification blocks should report `[PASÓ]`.

### 4.3 Program Entry Points: `main` (Hosted) vs. `test_main` (Freestanding)
Zs supports two distinct execution modes with different entry points:
- **Hosted Mode (Default):** The program links against the standard C runtime via `gcc -pie`. The entry point function must be named `main` and return `i32`.
- **Freestanding Mode (`--freestanding`):** The program links without libc via `gcc -nostdlib -pie` using `runtime/zs_start.s`. The entry point function must be named `test_main` and return `i32`. Its return value is passed directly to the Linux kernel via `sys_exit_group` (syscall 94).

#### Hosted "Hello, World!" Example
Create a file named `hello.Zs`:
```zs
// hello.Zs - Hosted execution linking libc
foreign {
    fn std_println(buf: loc<u8>, len: i32);
}

fn main() -> i32 {
    anchor msg: u8[13] = [72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100, 33]; // "Hello, world!"
    val p = msg.start;
    std_println(p, 13);
    return 0;
}
```
*(Note: `foreign { ... }` is the primary syntax for external declarations; `foreign "C" { ... }` is also accepted as a valid variant).*

Compile and run the program:
```bash
./build/zsc hello.Zs -o hello
./hello
```
Output:
```text
Ejecutable compilado con éxito: hello
Hello, world!
```
Exit code: `0`.

#### Freestanding Userspace Example (No libc)
Create a file named `freestanding.Zs`:
```zs
// freestanding.Zs - Standalone userspace Linux execution without libc
fn test_main() -> i32 {
    // Return value feeds directly into sys_exit_group (syscall 94)
    return 42;
}
```

Compile with `--freestanding` and run:
```bash
./build/zsc freestanding.Zs --freestanding -o freestanding
./freestanding
echo "Process exit code: $?"
```
Output:
```text
Ejecutable compilado con éxito: freestanding
Process exit code: 42
```

### 4.4 Compiler Driver Options
The `zsc` compiler driver translates `.Zs` files, emits assembly, and invokes system tools (`as` for object generation, and `gcc` for executable linking):

| Flag | Description | Verified Command Example |
| :--- | :--- | :--- |
| `-o <file>` | Specify output path (binary, `.s`, or `.o`) | `./build/zsc hello.Zs -o hello` |
| `--emit-asm` | Emit GNU ARM64 assembly (`.s`) and exit | `./build/zsc hello.Zs --emit-asm -o hello.s` |
| `-c`, `--emit-obj` | Emit relocatable ELF64 object file (`.o`) via `as` | `./build/zsc hello.Zs -c -o hello.o` |
| `--freestanding` | Link binary with `runtime/zs_start.s` via `gcc -nostdlib -pie` (entry `test_main`). *Verified manually; not invoked in automated runner*. | `./build/zsc app.Zs --freestanding -o app` |
| `-h`, `--help` | Display CLI usage information | `./build/zsc --help` |

---

## 5. Language Tour

> [!WARNING]
> **Avoid for Now:**
> The following language constructs currently have verified backend defects or omissions and should be avoided in production code:
> - **Compound assignments (`+=`, `-=`, `*=`, `/=`):** Use explicit long-form assignment `x = x + 1` ([L01](#l01-compound-assignment-overwrites---etc-silent)).
> - **Narrowing casts (`as`):** Use bitwise masks `x & 255` instead of `x as u8` ([L04](#l04-typecast-as-does-not-truncate-or-sign-extend-silent)).
> - **`raw { ... }` blocks:** Do not place executable statements inside `raw` blocks ([L06](#l06-raw----blocks-discard-inner-statements-silent)).
> - **`break` and `continue`:** Statements are ignored in codegen; using `break` in `loop` causes infinite hangs ([L16](#l16-break-and-continue-ignored-in-codegen-silent)).
> - **`choice` / `branch`:** Branch bodies are omitted in codegen; pattern matching falls through ([L17](#l17-branch-statement-body-omitted-in-codegen-silent)).
> - **`record` instantiation:** Instantiation evaluates to `void`; field access on variables fails ([L19](#l19-record-instantiation-and-field-access-limitations-silently-void--loud)).
> - **`walk .. by k`:** The `by k` step clause is ignored in codegen; iteration always steps by 1 ([L18](#l18-walk--by-k-step-clause-ignored-in-codegen-silent)).
> - **Floating-point types (`f32`, `f64`):** Machine floating-point code generation is not implemented ([Section 5.2](#52-primitive-types)).
> - **`if` as an expression:** `if` is only valid as a statement, not an expression.
> - **Overflow & Division by zero:** Do not assume wrapping or trapping; arithmetic evaluates in 64 bits and `x / 0` returns 0 ([L20](#l20-arithmetic-evaluation-in-64-bit-registers-and-division-by-zero-silent)).

Each feature in this tour is labeled with one of the four project status tags:
- **Verified**: Appears in a test that compiles, executes, and verifies a result.
- **Implemented, not covered by tests**: Functional when tested independently, but not covered by an automated execution test in `tests/test_31`–`test_60`.
- **Parsed only**: Accepted by the parser and type checker, but not implemented in code generation.
- **Design only**: Appears in the design specification but does not exist in the compiler.

### 5.1 Syntax and Structure
A Zs file belongs to a module and may import other modules or declare foreign ABI interfaces.

```zs
module my_module;

use other_module;

// Primary foreign block syntax
foreign {
    fn sys_write(fd: i32, buf: loc<u8>, count: i32) -> i32;
}

// foreign "C" is also accepted as a valid variant
foreign "C" {
    fn sys_close(fd: i32) -> i32;
}
```

### 5.2 Primitive Types

| Type | Status | Description & Backend Behavior |
| :--- | :--- | :--- |
| `i32` | **Verified** | 32-bit signed integer. Default type for integer literals. Verified across tests 31–50. |
| `i64` | **Verified** | 64-bit signed integer. Verified in Test 36 and Test 54. |
| `u8` | **Verified** | 8-bit unsigned integer (byte). Verified in Tests 51, 56, and 57. |
| `bool` | **Implemented, not covered by tests** | Boolean (`true`, `false`). Verified to compile and return `1` independently; no test in 31–60 uses `bool` in execution. |
| `i8`, `i16` | **Implemented, not covered by tests** | 8-bit and 16-bit signed integers. Verified to compile and return `42` independently; not used in tests 31–60. |
| `u16`, `u32`, `u64` | **Implemented, not covered by tests** | Unsigned integers. Verified to compile and return `42` independently; not used in tests 31–60. |
| `isize`, `usize` | **Implemented, not covered by tests** | Pointer-sized integers (64-bit on AArch64). Verified to compile and return `42` independently; not used in tests 31–60. |
| `()` | **Implemented, not covered by tests** | Unit type. Functions without explicit return values default to unit. |
| `f32`, `f64` | **Parsed only** | 32-bit and 64-bit floating point. Accepted by lexer and checker, but the code generator emits integer registers and `0` for float literals. Floating point instructions are not implemented. |

### 5.3 Variables and Immutability
Variables in Zs are explicitly declared as immutable bindings (`val`) or mutable variables (`var`):

```zs
fn test_main() -> i32 {
    val a: i32 = 10; // Immutable constant binding [Verified]
    var b: i32 = 20; // Mutable variable [Verified]
    
    b = b + a;
    return b; // returns 30
}
```
Attempting to reassign to a `val` variable results in a compile-time rejection:
```text
error semántico: tests/test_21_val_immutable.Zs:5:5: No se puede reasignar la variable inmutable 'val' 'x'
```

### 5.4 Explicit Type Conversions (`as`) [Implemented, not covered by tests]
Zs forbids implicit type coercions. Types must be converted explicitly using the `as` operator.

> [!WARNING]
> In the current code generation backend, `as` changes the type for the type checker but **does not emit machine instructions for truncation or sign-extension** (see [L04](#l04-typecast-as-does-not-truncate-or-sign-extend-silent)).

We can demonstrate this lack of truncation through an in-program comparison:
```zs
fn test_main() -> i32 {
    val x: i32 = 258;
    val y: u8 = x as u8;
    
    // If truncation occurred, (258 % 256) would equal 2.
    // Because no truncation instructions are emitted, y remains 258.
    if ((y as i32) == 258) {
        return 1; // Evaluates to true, confirming no truncation occurred
    }
    return 0;
}
```
Compiling and executing this program returns exit code `1`, proving inside the program that `y` retained the value `258`.

### 5.5 Dual Return Error Handling [Verified]
Functions indicate error states using the dual return syntax `-> T or E`. Errors are triggered via `fail <code-or-val>;`:

```zs
fn divide(a: i32, b: i32) -> i32 or i32 {
    if (b == 0) {
        fail 55; // Sets Carry = 1, error code in x1
    }
    return a / b; // Sets Carry = 0, payload in x0
}

fn step() -> i32 or i32 {
    // Propagate failure to caller if divide() fails
    val x = divide(10, 0) or return fail;
    return x + 10;
}

fn test_main() -> i32 {
    // Catch failure and extract error code
    val res = step() catch(err) {
        return err; // Returns 55
    };
    return res;
}
```
Executing this exact snippet produces exit code `55`.

**Hardware Mechanism:** Dual return is implemented using the ARM64 CPU Carry flag (`C`):
- **Success:** The function clears Carry (`adds xzr, xzr, xzr`), payload in `x0`.
- **Failure:** The function sets Carry (`cmp xzr, xzr`), error in `x1`.
- **Caller branching:** The caller checks the condition using `b.cs` (Branch if Carry Set).

### 5.6 Resource Cleanup (`defer`) [Verified]
The `defer` statement schedules code to be executed when exiting the enclosing lexical scope or function, following Last-In-First-Out (LIFO) order:

```zs
fn test_main() -> i32 {
    anchor buf: i32[1] = [0];
    val p = buf.start;
    {
        defer { *p = *p + 10; } // Executed 3rd: 14 + 10 = 24
        defer { *p = *p * 2; }  // Executed 2nd: 7 * 2 = 14
        defer { *p = *p + 5; }  // Executed 1st: 2 + 5 = 7
        *p = 2;
    }
    return buf[0]; // returns 24
}
```
*(From `tests/test_47_defer_lifo_order.Zs`).*

---

## 6. The Pointer and Memory Model (TAD)

Zs models memory through **Topology-Anchored Domains (TAD)**. Rather than relying on raw untyped pointers, spatial pointers are bound to bounded memory anchors allocated on the stack.

### 6.1 Anchors, Locators, and Views

#### 1. Anchors (`anchor`) [Verified]
An anchor reserves memory on the stack with fixed base and limit addresses:
```zs
anchor a: i32[4] = [10, 20, 30, 40];
```
Properties:
- `a.start` (**Verified**): Produces a `loc<T>` pointing to element 0 (`base`).
- `a.end` (**Implemented, not covered by tests**): Produces a `loc<T>` pointing to the past-the-end boundary (`limit`).
- `a.len` (**Implemented, not covered by tests**): Evaluates to the compile-time element count (`4`).

#### 2. Locators (`loc<T>`) [Verified]
From reading the compiler source code (`src/zs_codegen_arm64.c`), a locator is implemented as a 24-byte fat pointer triplet represented on the stack:
- `ptr` (8 bytes): Current memory address.
- `base` (8 bytes): Lower bound of the anchor domain.
- `limit` (8 bytes): Upper bound of the anchor domain (`base + len * sizeof(T)`).

#### 3. Views (`view<T>`) [Verified]
From reading the compiler source code (`src/zs_codegen_arm64.c`), a view is implemented as a 16-byte slice structure on the stack:
- `ptr` (8 bytes): Current start address.
- `len` (8 bytes): Number of elements in the slice.

### 6.2 TAD Navigation Operators

Navigation along memory domains is performed using dedicated TAD operators.

| Operator | Syntax | Status | Behavior & Verification |
| :--- | :--- | :--- | :--- |
| Forward Navigation | `p ~> n` | **Verified** | Moves pointer forward by `n` elements. Emits `cmp x0, x2; b.hi .Lzs_trap`. Allows `ptr == limit` without trapping (see [L03](#l03-one-past-the-end-navigation-boundary-loud-on-read--silent-on-write)). Verified in Test 32. |
| Backward Navigation | `p <~ n` | **Verified** | Moves pointer backward by `n` elements. Emits `cmp x0, x1; b.lo .Lzs_trap`. Traps if `ptr < base`. Verified in Test 33. |
| Bound-Checked Forward | `p ?~> n` | **Verified** | Guarded forward navigation returning `(loc<T> or fail)`. Used with `or` fallback. Verified in Test 34. |
| Toroidal / Ring Buffer | `p %~> n` | **Verified** | Modular navigation wrapping within `[base, limit)`. In Test 35, `p %~> 5` returns element 1 (`2`). Handling for `k=0`, `k=N`, `k=2N+1`, and negative `k` was confirmed in manual standalone experiments. |
| Aligned Byte Step | `p ~># n` | **Verified** | Advances pointer by exact byte offset `n`. Verified in Test 36. |
| Coordinate Metric | `p1 <=> p2` | **Verified** | Computes signed distance in elements between two pointers: `(p1 - p2) / sizeof(T)`. Verified in Test 37. |
| Slicing Operator | `a[start .. end]` | **Verified** | Creates a `view<T>`. Only constant integer literals are supported as bounds in codegen (see [L15](#l15-dynamic-slice-bounds-unsupported-silent)). Verified in Test 38. |
| Iteration Walker | `walk x in a` | **Verified** | Iterates sequentially through all elements of an anchor. Stepping clause `by k` is ignored in codegen (see [L18](#l18-walk--by-k-step-clause-ignored-in-codegen-silent)). Verified in Test 39. |

### 6.3 Critical Safety Distinction: Reads vs. Writes

> [!CAUTION]
> **Spatial Memory Safety Asymmetry:**
> 
> - **READS ARE BOUNDS-CHECKED:** Reading memory through `p ~> n`, dereferencing `*p`, or indexing `a[i]` generates comparison checks against `limit` and `base`. If out-of-bounds, the program terminates immediately via the ARM64 `brk #0x42` trap instruction (`SIGTRAP`, signal 5).
> - **WRITES ARE UNCHECKED:** In the current code generation backend, memory writes through indexed expressions (`a[i] = val`) or pointer stores (`*p = val`) emit direct store instructions (`str`) **without generating bounds comparisons** (see [L02](#l02-out-of-bounds-memory-writes-are-unchecked-silent)). Out-of-bounds writes will corrupt surrounding stack memory without raising a trap.

---

## 7. Verified Vocabulary and the 31–60 Method

### 7.1 Conservative Language Culture
In Zs, the recommended development culture is deliberately conservative: **build software supported by the constructions that tests 31 to 60 already use, and learn on that foundation.**

- **What counts as verified:** Only constructions that appear in a test that compiles, executes, and verifies a result. Tests 01–30 verify front-end components (lexer, parser, checker) and do not count as execution verification (for example, `+=` appears in Test 16, which only checks the parser).
- **Test Directory Structure:** The `tests/` directory contains 62 files:
  - 60 active tests executed by `make test-all` (10 per block across Blocks 1 to 6).
  - 1 helper module (`tests/math_mod.Zs`) used by Test 59.
  - 1 orphaned test file (`tests/test_59_cli_freestanding_elf.Zs`) not executed by the test runner.
- **Rules by project size:**
  - *Small and medium projects:* Staying within the comfort zone of tests 31–60 is sufficient.
  - *Large projects:* This is a separate category requiring intentional experimentation. It is the responsibility of whoever builds it. If a medium project accidentally grows large, that is the signal to begin structured experimentation.
  - Anyone writing software intended for distribution must know which language areas are verified and which are not.

### 7.2 The `experiments/` Directory and the 4-Step Method
It is strongly recommended to maintain an `experiments/` (or `lxp`) directory within your project. Experiments that work should be retained as personal regression tests rather than discarded.

The author's 4-step experimental method:
1. **Run the test as-is:** Ensure the baseline compiles and runs.
2. **Change ONE single thing:** Isolate the variable or syntax under test.
3. **Predict before executing:** Write down what you expect to happen before running.
4. **Execute, compare, and inspect assembly:** If the result does not match your prediction, inspect the generated assembly using `./build/zsc file.Zs --emit-asm -o file.s`.

Each experiment has two essential halves:
- **The happy path:** Normal, expected inputs.
- **The abuse path:** Forcing boundary edges (e.g., index equal to length, values that overflow). The most valuable lessons in Zs come from observing what the compiler does when something goes wrong. If it remains silent, you must monitor it manually.

> [!TIP]
> When adapting tests 31–60, changing indices in memory reads is safe because bounds violations are caught at runtime. Changing indices in memory writes is **not safe** ([L02](#l02-out-of-bounds-memory-writes-are-unchecked-silent)), so you must verify manually that your writes stay within the anchor boundaries.

### 7.3 Construct-to-Test Mapping Table

| Feature / Construct | Verifying Test Suite | Test File | Verified Execution Behavior |
| :--- | :--- | :--- | :--- |
| Stack Anchor Allocation | Block 4 (Codegen) | `tests/test_31_anchor_alloc_stack.Zs` | Allocates array, initial element access returns `10`. |
| Forward Step `~>` | Block 4 (Codegen) | `tests/test_32_fwd_displacement.Zs` | Displaces pointer `p ~> 3`, returns value `40` (subject to [L03](#l03-one-past-the-end-navigation-boundary-loud-on-read--silent-on-write)). |
| Backward Step `<~` | Block 4 (Codegen) | `tests/test_33_bwd_displacement.Zs` | Traps with `SIGTRAP` on underflow past anchor origin. |
| Safe Fallback `?~>` | Block 4 (Codegen) | `tests/test_34_safe_displacement_fallback.Zs` | Recovers fallback `(p ?~> 10) or p`, returns `100`. |
| Ring Modulo `%~>` | Block 4 (Codegen) | `tests/test_35_toroidal_ring_buffer.Zs` | Wraps around 4-element ring buffer, returns `2`. |
| Byte Displacement `~>#` | Block 4 (Codegen) | `tests/test_36_aligned_byte_step.Zs` | Steps 8 bytes on `i64` anchor, returns `200`. |
| Distance Metric `<=>` | Block 4 (Codegen) | `tests/test_37_coordinate_metric.Zs` | Evaluates element distance `7 - 2`, returns `5`. |
| View Slicing `a[s .. e]` | Block 4 (Codegen) | `tests/test_38_view_slicing.Zs` | Slices `a[2 .. 5]`, reads `sub[1]`, returns `40` (subject to [L15](#l15-dynamic-slice-bounds-unsupported-silent)). |
| Walker Loop `walk` | Block 4 (Codegen) | `tests/test_39_walk_iteration.Zs` | Accumulates `10 + 20 + 30 + 40`, returns `100` (step clause subject to [L18](#l18-walk--by-k-step-clause-ignored-in-codegen-silent)). |
| Hardware Trap `brk #0x42` | Block 4 (Codegen) | `tests/test_40_trap_out_of_bounds.Zs` | Access `p ~> 100` triggers `brk #0x42` (SIGTRAP). |
| Dual Return Success | Block 5 (Dual/Defer) | `tests/test_41_dual_return_success.Zs` | Returns payload with Carry = 0 (`adds xzr, xzr, xzr`). |
| Dual Return Failure | Block 5 (Dual/Defer) | `tests/test_42_dual_return_fail.Zs` | Emits `fail 42;` setting Carry = 1 (`cmp xzr, xzr`). |
| Propagation `or return fail` | Block 5 (Dual/Defer) | `tests/test_43_or_propagate.Zs` | Bubbles failure to caller up the call stack via `b.cs`. |
| Fallback `or <val>` | Block 5 (Dual/Defer) | `tests/test_44_or_fallback_val.Zs` | Substitutes default value upon Carry = 1. |
| `catch` Block Handling | Block 5 (Dual/Defer) | `tests/test_45_catch_block.Zs` | Handles failure block without unhandled traps. |
| Function Exit `defer` | Block 5 (Dual/Defer) | `tests/test_46_defer_function_exit.Zs` | Executes deferred mutation on function exit. |
| LIFO Execution Order | Block 5 (Dual/Defer) | `tests/test_47_defer_lifo_order.Zs` | 3 deferred statements execute in reverse order (24). |
| `defer` on Failure Exit | Block 5 (Dual/Defer) | `tests/test_48_defer_on_fail.Zs` | Runs deferred cleanups when exiting via `fail`. |
| `defer` in Nested Scope | Block 5 (Dual/Defer) | `tests/test_49_defer_in_loop_break.Zs` | Executes deferred block upon exiting nested scope `{ ... }` inside `while` (while subject to [L16](#l16-break-and-continue-ignored-in-codegen-silent)). |
| Register Preservation | Block 5 (Dual/Defer) | `tests/test_50_register_preservation.Zs` | Preserves callee-saved registers across calls. |
| Linux `sys_write` Syscall | Block 6 (Runtime/IO) | `tests/test_51_direct_sys_write.Zs` | Emits AArch64 `svc #0` syscall 64 to stdout. |
| Linux `sys_read` Syscall | Block 6 (Runtime/IO) | `tests/test_52_direct_sys_read.Zs` | Executes syscall 63 reading from pipe buffer. |
| Linux File Open/Close | Block 6 (Runtime/IO) | `tests/test_53_file_open_close.Zs` | Calls `sys_openat` (56) and `sys_close` (57). |
| Integer Formatting | Block 6 (Runtime/IO) | `tests/test_54_formatted_i64_str.Zs` | Formats `i64` integer into ASCII string buffer. |
| Standalone `_start` Entry | Block 6 (Runtime/IO) | `tests/test_55_freestanding_start.Zs` | Pure ELF execution with `sys_exit_group` (94). |
| High-level `std_println` | Block 6 (Runtime/IO) | `tests/test_56_high_level_print.Zs` | Writes buffer followed by newline to stdout. |
| File Read/Write Persistence | Block 6 (Runtime/IO) | `tests/test_57_file_read_write.Zs` | Complete file open, write, reopen, read, and close cycle. |
| Driver Assembly Emission | Block 6 (Runtime/IO) | `tests/test_58_cli_driver_asm.Zs` | Emits valid `.s` assembly via `--emit-asm`. |
| Multi-module Compilation | Block 6 (Runtime/IO) | `tests/test_59_module_multitranslation.Zs` | Compiles separate `.o` modules and links with `tests/math_mod.Zs`. |
| Full Integration Pipeline | Block 6 (Runtime/IO) | `tests/test_60_full_system_integration.Zs` | End-to-end multi-feature execution pipeline. |

---

### 7.4 Language Features Not Covered by Execution Tests

The following constructs exist in the compiler front-end or grammar, but are either unverified by tests 31–60 or unsupported by the code generator:

- **`while` loop:** **Verified** in Test 49, but `break` and `continue` are **Parsed only** (the code generator omits branch statements for them; see [L16](#l16-break-and-continue-ignored-in-codegen-silent)).
- **`loop { ... }`:** **Implemented, not covered by tests** (verified independently to compile and loop correctly; no test in 31–60 executes it; `break` inside it causes an infinite hang).
- **`walk x in a by k`:** `walk` is **Verified** in Test 39, but the `by k` step clause is **Parsed only** (the code generator hardcodes the step to 1 element; see [L18](#l18-walk--by-k-step-clause-ignored-in-codegen-silent)).
- **`record` declarations:** **Parsed only** (parses struct layout in Test 12, but instantiation yields `void` and field access fails; see [L19](#l19-record-instantiation-and-field-access-limitations-silently-void--loud)).
- **`choice` / `branch`:** **Parsed only** (`choice` and `branch` parse and type-check exhaustiveness in front-end tests 13, 17, and 25, but the code generator omits `branch` handling; see [L17](#l17-branch-statement-body-omitted-in-codegen-silent)).
- **`if` as expression:** **Design only** (syntax error; `if` is only parsed as a statement).

---

## 8. Known Limits and Caveats

The 20 verified limits of the current compiler are indexed below with stable identifiers (`L01`–`L20`). They are divided between **Silent** behaviors (the code compiles cleanly but produces incorrect state, ignores logic, or corrupts memory) and **Loud** behaviors (the compiler rejects the code with an error, the assembler warns/fails, or hardware raises a trap).

### Summary: Silent vs. Loud Behaviors

| ID | Category | Description | Severity & Impact |
| :--- | :--- | :--- | :--- |
| **L01** | **Silent** | Compound assignment (`+=`, `-=`, etc.) compiles to simple assignment | Silently discards existing variable value |
| **L02** | **Silent** | Out-of-bounds writes (`*p = v`, `a[i] = v`) are unchecked | Silently corrupts adjacent stack variables |
| **L04** | **Silent** | Typecast `as` narrows type without machine truncation | Silently preserves un-truncated bits in register |
| **L05** | **Silent** | Integer-to-locator cast leaves garbage base/limit on stack | Silently creates invalid spatial bounds |
| **L06** | **Silent** | `raw { ... }` block statements are omitted in codegen | Silently drops statements inside block |
| **L07** | **Silent** | Unreferenced `use non_existent;` produces no error | Silently ignores missing import if unreferenced |
| **L11** | **Silent** | Foreign C ABI signature mismatch bypasses bounds | Silently bypasses safety checks |
| **L12b**| **Silent** | String literal `\\` mutates to backspace (`0x08`) | Emits `.asciz "a\b"`, mutating byte from `0x5C` to `0x08` in `.rodata` |
| **L15** | **Silent** | Dynamic slice bounds `a[i .. j]` default to `0 .. len` | Silently ignores runtime variable indices |
| **L16** | **Silent** | `break` and `continue` ignored in codegen | Loop with `break` hangs infinitely (exit 124); `continue` does not skip |
| **L17** | **Silent** | `branch` statement body omitted in codegen | Silently falls through without executing matching pattern branch |
| **L18** | **Silent** | `walk .. by k` clause ignored in codegen | Silently steps by 1 element regardless of `k` |
| **L19a**| **Silent** | `val p = Point { ... }` untyped binding produces `void` | Compiles cleanly with exit 0, but variable type is inferred as `void` |
| **L20** | **Silent** | Arithmetic in 64-bit registers & division by zero | Overflow evaluates in 64 bits without wrap/trap; `10 / 0` returns `0` |
| **L03** | **Loud / Silent** | Past-the-end `*p` at `limit` | **Loud on read** (SIGTRAP exit 133) / **Silent on write** (exit 0) |
| **L08** | **Loud** | Typed integer suffixes (`44_u8`) assigned without `as` | Type checker rejection |
| **L09** | **Loud** | Calling `.view()` method syntax | Parser syntax rejection |
| **L10** | **Loud** | Declaring `var` or `anchor` at module scope | Parser syntax rejection |
| **L12a**| **Loud** | String literal `\"` causes GNU `as` assembly error | Emits unescaped `.asciz "He said "hi""`, failing assembly |
| **L12c**| **Loud** | String literal `\n` causes GNU `as` assembly warning | Emits raw newline in `.asciz`, triggering `Warning: unterminated string` |
| **L12d**| **Clean**| String literal `\t` emits tab byte cleanly | Emits tab byte `0x09` into `.rodata` without errors |
| **L13** | **Loud** | Fixed-address anchors and volatile accesses not supported | Keywords do not exist in lexer/parser |
| **L14** | **Loud** | Concurrency primitives not implemented | Primitives do not exist in language |
| **L19b**| **Loud** | `val p: Point = Point { ... }` or reading `p.x` | Fails type check (`esperado Point, recibido void` or `no es record`) |

---

### Detailed Analysis of Known Limits

#### L01: Compound Assignment Overwrites (`+=`, `-=`, etc.) [Silent]
- **Issue:** Compound operators (`+=`, `-=`, `*=`, `/=`) compile into simple assignments, discarding the existing value of the variable.
- **Empirical Proof:** Executing `var x = 10; x += 5; return x;` returns `5`, not `15`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_stmt` (search: `case STMT_ASSIGN:`).
- **Workaround:** Always use explicit long-form assignment: `x = x + 5;`.

#### L02: Out-of-Bounds Memory Writes are Unchecked [Silent]
- **Issue:** Pointer store operations (`*p = val`) and array write assignments (`a[i] = val`) emit direct `str` instructions without bounds comparisons against `limit`.
- **Empirical Proof:** Writing `a[4] = 999;` on an anchor of size 4 succeeds silently without triggering `brk #0x42`, whereas reading `val x = a[4];` triggers `SIGTRAP`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_stmt` (search: `case STMT_ASSIGN:`).
- **Workaround:** Verify locator bounds manually before storing to memory.

#### L03: One-Past-The-End Navigation Boundary [Loud on Read / Silent on Write]
- **Issue:** The forward displacement operator `~>` validates limits using `cmp x0, x2; b.hi .Lzs_trap`. Because it tests unsigned *greater than* (`b.hi`), navigating to `ptr == limit` does not trap during pointer calculation.
- **Empirical Proof Across 3 Cases (on `anchor a: i32[4]`):**
  - **Forming the locator:** `val p = a.start ~> 4; return 0;` exits with code `0` (does not trap; `b.hi` allows `ptr == limit`).
  - **Reading through the locator:** `val p = a.start ~> 4; return *p;` emits `cmp x0, x2; b.hs .Lzs_trap`, trapping with `SIGTRAP` (exit code `133`).
  - **Writing through the locator:** `val p = a.start ~> 4; *p = 99; return 0;` emits unvalidated `str w1, [x0]`, exiting with code `0` (does not trap, corrupts stack).
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_expr` (search: `if (op == TOK_TAD_FWD)`).
- **Workaround:** Ensure displacement indices remain within `0 .. (len - 1)` before dereferencing.

#### L04: Typecast (`as`) Does Not Truncate or Sign-Extend [Silent]
- **Issue:** The `as` cast operator validates types in the checker but emits no machine truncation or sign-extension instructions in the code generator.
- **Empirical Proof:** In `val x: i32 = 258; val y: u8 = x as u8;`, testing `if ((y as i32) == 258) return 1;` returns `1`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_expr` (search: `case EXPR_CAST:`).
- **Workaround:** Apply bitwise masking manually: `val y = (x & 255) as u8;`.

#### L05: Integer-to-Locator Cast Outside `raw` Blocks [Silent]
- **Issue:** Casting an integer to a locator (`addr as loc<T>`) is permitted outside `raw` blocks by the type checker. The code generator loads `addr` into `ptr` (`x0`), leaving `base` (`x1`) and `limit` (`x2`) uninitialized with stack register garbage.
- **Empirical Proof:** Reading from `val p = 0x1000 as loc<i32>;` dereferences uninitialized bounds.
- **Source Reference:** `src/zs_checker.c` in `check_expr` (search: `case EXPR_CAST:`) and `src/zs_codegen_arm64.c` in `codegen_expr` (search: `case EXPR_CAST:`).
- **Workaround:** Only create locators via anchor properties (`a.start`, `a.end`).

#### L06: `raw { ... }` Blocks Discard Inner Statements [Silent]
- **Issue:** The code generator does not emit code for statements enclosed within `raw { ... }` blocks.
- **Empirical Proof:** In `var x = 10; raw { x = 99; } return x;`, the return value is `10`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_stmt` (omitted `case STMT_RAW:`).
- **Workaround:** Avoid placing executable statements inside `raw { ... }` blocks in this version.

#### L07: Unreferenced `use` Imports Produce No Error [Silent]
- **Issue:** Importing a non-existent module name via `use non_existent_mod;` produces no error if symbols from that module are not referenced.
- **Source Reference:** `src/zs_parser.c` in `parse_use_decl` (search: `parse_use_decl`).

#### L08: Numeric Literals Default to `i32` [Loud]
- **Issue:** All integer literals without explicit casting default to type `i32`. Suffixes such as `44_u8` are parsed lexically but rejected during type checking if assigned to a `u8` without an explicit `as u8`.
- **Source Reference:** `src/zs_checker.c` in `check_expr` (search: `case EXPR_INT_LIT:`).
- **Workaround:** Use explicit type casts: `val x: u8 = 44 as u8;`.

#### L09: Method Call `.view()` Fails to Parse [Loud]
- **Issue:** Calling `a.view()` fails to parse because `view` is reserved as a type keyword.
- **Source Reference:** `src/zs_parser.c` in `parse_primary_expr` (search: `TOK_KW_VIEW`).
- **Workaround:** Use bracket slice syntax: `val v = a[0 .. 4];`.

#### L10: Module-Level Global `var` and `anchor` are Disallowed [Loud]
- **Issue:** Declaring `var` or `anchor` at file/module scope produces a syntax error: `Declaración de alto nivel esperada`.
- **Source Reference:** `src/zs_parser.c` in `parse_top_level_decl` (search: `parse_top_level_decl`).
- **Workaround:** Declare all anchors and variables inside functions.

#### L11: `foreign` ABI Escape Hatch [Silent]
- **Issue:** Foreign C functions declared via `foreign { ... }` or `foreign "C" { ... }` bypass all bounds and safety checks.
- **Source Reference:** `src/zs_checker.c` in `check_foreign_block` (search: `check_foreign_block`).
- **Workaround:** Audit foreign signatures against system C headers.

#### L12: Unescaped String Literal Code Generation [Loud / Mutating]
- **Issue:** String literals in expressions are emitted directly into `.asciz "%s"` without re-escaping special characters:
  - **Double quotes (`\"`, L12a - Loud):** Evaluating `"He said \"hi\""` emits `.asciz "He said "hi""`. GNU `as` fails with `Error: junk at end of line, first unrecognized character is 'h'`.
  - **Backslashes (`\\`, L12b - Silent):** Evaluating `"a\\b"` emits `.asciz "a\b"`. GNU `as` interprets `\b` as a backspace escape, mutating `.rodata` bytes from backslash (`0x5C`) to backspace (`0x08`).
  - **Newlines (`\n`, L12c - Loud):** Evaluating `"Hello, world!\n"` emits a raw newline byte in `.asciz`, splitting the line. GNU `as` emits a single warning: `Warning: unterminated string; newline inserted`, but automatically inserts the `0x0A` byte into `.rodata`.
  - **Tabs (`\t`, L12d - Clean):** Evaluating `"a\tb"` emits a raw tab byte `0x09`, which GNU `as` accepts into `.rodata` without errors.
- **Source Reference:** `src/zs_codegen_arm64.c` in `zs_codegen_generate` (search: `.asciz`).
- **Workaround:** Use byte array anchors (`anchor msg: u8[...] = [...]`) for buffers containing quotes, backslashes, or newlines, or keep string literals single-line and unquoted.

#### L13: Fixed-Address Anchors and Volatile Accesses Not Supported [Loud]
- **Issue:** There is no supported syntax or mechanism to declare memory-mapped anchors at fixed physical addresses or to emit volatile memory accesses.
- **Source Reference:** `src/zs_lexer.c` and `src/zs_parser.c`.

#### L14: Concurrency Primitives Not Implemented [Loud]
- **Issue:** The compiler does not support threading, atomic operations, or mutexes. Data race analysis is not applicable.
- **Source Reference:** `src/` (no concurrency primitives implemented).

#### L15: Dynamic Slice Bounds Unsupported [Silent]
- **Issue:** Slicing expressions `a[start .. end]` only evaluate integer literal constants (`EXPR_INT_LIT`). Passing variable identifiers causes the code generator to default bounds to `0` and `anchor_len`.
- **Empirical Proof:** On `anchor a: i32[8] = [10, 20, 30, 40, 50, 60, 70, 80]`, evaluating `var i = 2; var j = 5; val s = a[i .. j]; return s[1];` returns `20` instead of `40`, because `s` silently defaulted to `a[0 .. 8]`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_expr` (search: `case EXPR_INDEX:`).
- **Workaround:** Only use literal integer bounds in slice expressions: `a[1 .. 3]`.

#### L16: `break` and `continue` Ignored in Codegen [Silent]
- **Issue:** The code generator does not implement statement visitors for `break` (`STMT_BREAK`) or `continue` (`STMT_CONTINUE`).
- **Empirical Proof:**
  - Executing `loop { break; }` does not exit; it hangs indefinitely until killed (`timeout 2` exits with code `124`).
  - Executing `while (true) { break; }` hangs indefinitely (`timeout 2` exits with code `124`).
  - In a `while` loop with `if (i == 2) continue; sum = sum + 10;`, `continue` is ignored, accumulating `30` instead of `20`.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_stmt` (omitted `case STMT_BREAK:` and `case STMT_CONTINUE:`).
- **Workaround:** Structure loops using boolean conditions in `while` and explicit conditional branches without relying on `break` or `continue`.

#### L17: `branch` Statement Body Omitted in Codegen [Silent]
- **Issue:** The code generator does not emit code for `branch` pattern-matching blocks (`STMT_BRANCH`).
- **Empirical Proof:** In `choice Color { Rojo; Azul; }`, executing `val c = Color::Rojo; branch c { Color::Rojo => return 10; Color::Azul => return 20; } return 30;` compiles cleanly and returns `30` (both pattern arms are skipped).
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_stmt` (omitted `case STMT_BRANCH:`).
- **Workaround:** Use cascading `if` / `else if` chains over integer constants rather than `branch`.

#### L18: `walk .. by k` Step Clause Ignored in Codegen [Silent]
- **Issue:** The code generator hardcodes the index step in `walk` loops to `#1` (`emit(cg, " add x10, x10, #1\n");`), ignoring the `by k` step expression.
- **Empirical Proof:** On `anchor a: i32[4] = [10, 20, 30, 40]`, running `walk x in a by 2 { sum = sum + x; }` returns `100` (`10 + 20 + 30 + 40`) instead of `40` (`10 + 30`).
- **Source Reference:** `src/zs_codegen_arm64.c:1004` in `codegen_stmt` (`case STMT_WALK:`).
- **Workaround:** Use a manual `while` loop with explicit index stride arithmetic.

#### L19: `record` Instantiation and Field Access Limitations [Silently Void / Loud]
- **Issue:** The type checker does not implement a type handler for record literal expressions (`EXPR_RECORD_INIT`), returning `void`.
- **Empirical Proof:**
  - Untyped binding `val p = Point { x: 10, y: 20 };` compiles (exit 0), but `p` receives type `void` ([L19a](#summary-silent-vs-loud-behaviors)).
  - Explicitly typed binding `val p: Point = Point { ... };` fails with `esperado Point, recibido void` ([L19b](#summary-silent-vs-loud-behaviors)).
  - Reading a field `return p.x;` on an instantiated variable fails with `Acceso a campo en un tipo que no es record` ([L19b](#summary-silent-vs-loud-behaviors)).
  - On a typed function parameter `fn get_x(p: Point) -> i32 { return p.x; }`, field access passes type check, but codegen does not compute field offsets.
- **Source Reference:** `src/zs_checker.c` in `check_expr` (omitted `case EXPR_RECORD_INIT:`) and `src/zs_codegen_arm64.c`.
- **Workaround:** Store structured data in flat anchors or pass individual primitive arguments.

#### L20: Arithmetic Evaluation in 64-bit Registers and Division by Zero [Silent]
- **Issue:** Arithmetic operations on `i32` and `u8` are evaluated in 64-bit machine registers (`x` registers) without overflow traps or narrow truncation.
  - Adding `2147483647 + 1` evaluates to `2147483648` in 64 bits without wrapping to negative or trapping.
  - Adding `(200 as u8) + (100 as u8)` evaluates to `300` in 64-bit registers rather than wrapping to `44`.
  - Dividing by zero (`x / 0`) executes ARM64 `sdiv`, returning `0` silently without raising a trap.
- **Source Reference:** `src/zs_codegen_arm64.c` in `codegen_expr` (search: `case EXPR_BINARY:`).

---

### Implementation Notes: Stack Memory Layout
- **`loc<T>` Stack Footprint:** A `loc<T>` fat pointer occupies 24 bytes (3 64-bit words: `ptr`, `base`, `limit`) on the stack.
- **Source Reference:** `src/zs_codegen_arm64.c` in `scan_locals_in_stmts` (search: `scan_locals_in_stmts`).

---

## 9. Backward Compatibility Policy

> [!NOTE]
> **Planned Policy:** The `legacy` directive described below is a **planned policy** for future compiler versions and **does not yet exist in the current compiler**.

Zs prioritizes maintainability, scalable development, and backward compatibility over novelty:

1. **12-Month Major Updates:** Behavioral changes to the language or compiler will be restricted to a planned 12-month release cycle, preventing continuous churn.
2. **The `legacy` Directive Rule:** Every behavioral change between compiler versions will include a source-level `legacy` directive that restores the previous rule. Code written for a specific version can continue compiling without deep rewrites.
3. **Default Behavior:** The corrected behavior will always be the default; older behavior must be requested explicitly via the directive.
4. **Additive Changes:** Additive syntax additions (such as allowing both `foreign {` and `foreign "C" {`) do not alter existing behavior and do not require legacy directives. Both forms compile cleanly today.
5. **Trade-offs of Legacy Directives:** A legacy directive restores a grammatical or semantic rule; it does not guarantee the exact binary output (for example, local variable stack layout may change between compiler releases). Furthermore, enabling a legacy directive may reintroduce known insecure behaviors (such as unchecked memory writes).

---

## 10. Planned Fixes and Future Directions

### 10.1 Planned Fixes
The following compiler defect corrections are planned for future releases. Planned fixes will ship with their legacy directive:
- **[L01] Compound assignment code generation:** Emitting arithmetic operations before assignment in `+=`, `-=`, `*=`, `/=`.
- **[L02] Bounds checking for memory writes:** Emitting limit and base comparisons on store instructions (`*p = val` and `a[i] = val`).
- **[L04] Machine truncation for `as`:** Emitting `uxtb`, `uxth`, `sxtb`, and `sxth` machine instructions during type narrowing.
- **[L05] Integer-to-locator cast validation:** Restricting `addr as loc<T>` to `raw` blocks or initializing `base`/`limit` safely.
- **[L06] Code generation for `raw { ... }` blocks:** Implementing AST statement visitors for statements inside `raw`.
- **[L07] Module import validation for `use`:** Verifying existence of imported modules at declaration time.
- **[L09] Slicing method `.view()`:** Resolving parser keyword conflict to support method syntax alongside bracket slicing.
- **[L12] String literal escaping in `.asciz`:** Re-escaping quotes (`\"`), backslashes (`\\`), and newlines (`\n`) in assembly output.
- **[L15] Dynamic slicing expressions:** Calculating runtime register bounds for `a[start .. end]` when bounds are runtime variables.
- **[L16] Control flow statements:** Emitting branch jumps for `break` and `continue` inside `while` and `loop`.
- **[L17] Pattern matching `branch` statement:** Generating comparison trees or jump tables for `branch` arms.
- **[L18] Iteration stride in `walk`:** Emitting stride additions for the `by k` clause instead of hardcoding `#1`.
- **[L19] `record` support:** Implementing type checking and offset emission for record instantiation and field access.

### 10.2 Under Consideration (Needs Author Decision on Wrap vs. Trap)
- **[L20] Arithmetic overflow & division by zero semantics:** Author architectural decision required on whether overflow should trap (e.g. `brk #0x42`) or wrap via modular two's complement, and whether integer division by zero should raise an explicit trap.

### 10.3 Under Consideration, No Commitment
The following capabilities are under design evaluation without implementation commitment:
- **[L13] Fixed-address anchors and volatile access:** Providing dedicated abstractions for peripheral hardware mapping.
- **[L14] Concurrency primitives:** Exploring thread-safe abstractions and memory ordering models.
- **Floating-point code generation (`f32`/`f64`):** Emitting AArch64 floating-point register instructions (`d0`-`d31`, `fadd`, `fsub`).
- **Alternative Backends:** Evaluation of an x86_64 ELF backend and bare-metal ARM64 target profiles.

---

## 11. Reporting Issues and Verification Findings

When encountering discrepancies between compiler behavior and documented specifications, please submit a minimal reproducible test case.

### Bug Report Template
````text
Title: [Bug]: <Brief summary of defect>

Zs Compiler Commit: <git rev-parse HEAD>
System Architecture: <uname -a>

Source Code (minimal reproducible case):
```zs
// paste code here
```

Expected Behavior:
<What the documentation states>

Actual Behavior:
<Actual exit code, error message, or emitted assembly>

Verification Command:
$ ./build/zsc bug.Zs -o bug && ./bug
````

<!-- TODO(author): Insert repository issue tracker URL or contact email here -->

---

## 12. Repository Layout and License

### Directory Layout
```text
.
├── Makefile            # Build definitions for compiler and test suites
├── build/              # Build output directory (zsc binary, test executables)
├── experiments/        # Personal experiments and local tests (lxp)
├── include/            # C header files defining compiler data structures
│   ├── zs_arena.h      # Memory arena allocator
│   ├── zs_ast.h        # Abstract Syntax Tree nodes
│   ├── zs_checker.h    # Type checker and semantic analyzer
│   ├── zs_codegen_arm64.h # AArch64 machine code and assembly generator
│   ├── zs_lexer.h      # Lexical analyzer
│   ├── zs_parser.h     # Recursive descent parser
│   ├── zs_source.h     # Source file loader and span tracker
│   ├── zs_symtab.h     # Symbol table and lexical scoping
│   ├── zs_token.h      # Token definitions and keywords
│   └── zs_types.h      # Type representation system
├── runtime/            # Low-level assembly runtime
│   ├── zs_runtime.s    # Syscalls and standard I/O functions
│   └── zs_start.s      # Standalone _start entry point for freestanding binaries
├── src/                # Compiler implementation in C99
│   ├── zsc_main.c      # Compiler CLI driver entry point
│   └── zs_*.c          # Lexer, parser, checker, codegen, arena implementations
└── tests/              # Test files and suites
    ├── math_mod.Zs     # Helper math module for Test 59
    ├── test_01-10      # Block 1: Lexical analysis and tokenization
    ├── test_11-20      # Block 2: Grammar, syntax, and AST parsing
    ├── test_21-30      # Block 3: Semantic analysis and type checking
    ├── test_31-40      # Block 4: ARM64 code generation and TAD execution
    ├── test_41-50      # Block 5: Dual Return ABI, defer, and register preservation
    └── test_51-60      # Block 6: Syscalls, I/O, freestanding ELF, and CLI driver
```

### License

This project is licensed under the [GNU General Public License (GNU GPL)](https://www.gnu.org/licenses/).

The GNU General Public License is a copyleft license that guarantees end users the four freedoms to run, study, share, and modify the software. Any derivative works must be distributed under the same or equivalent license terms.
