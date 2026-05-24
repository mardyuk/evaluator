# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and run

```bash
meson setup build        # first time only
ninja -C build           # compile

build/nogo file.nogo                    # compile + run source
build/nogo compile file.nogo out.ngb   # compile to bytecode
build/nogo run out.ngb                 # run bytecode
build/nogo run out.ngb --debug         # step debugger
```

There is no test suite — test by running `.nogo` files manually.

## Architecture

The pipeline is strictly linear; each stage produces a single output passed to the next:

```
source text
  → Lexer        (char stream, line tracking)
  → Tokenizer    (Token structs with TokType)
  → Parser       (shunting-yard + recursive descent → AST)
  → Compiler     (AST → ByteCode)
  → VM           (executes ByteCode)
```

**Lexer / Tokenizer** (`lexer.hpp/cpp`, `tokenizer.hpp/cpp`): The Lexer is a raw char-stream with `advance()`, `peek()`, `peekNext()`. The Tokenizer wraps it and produces `Token {TokType, text, line}`. All token types are in `token.hpp`.

**SymbolTable** (`scope.hpp`, header-only): Tracks variables during parsing and compilation. Globals get a flat integer address (index into `_mem` in the VM). Locals get a negative offset from FP (frame pointer), e.g. -4, -8. Nested function scoping uses a hop count for `LOAD_OUTER`/`STORE_OUTER`.

**AST** (`ast.hpp/cpp`): The `Op` enum (uint8_t) doubles as both AST operation codes and VM opcodes — the compiler maps AST nodes directly to `Op` values. `BinNode::opCode()` does this mapping. `StmtNode` has a `line` field; `ASTNode` does not.

**Compiler** (`compiler.hpp/cpp`): Register allocator uses `allocReg()`/`freeReg()` with a free-list. `SP = reg[2]`, `FP = reg[8]` — these are skipped by `allocReg()`. Instructions are 4 bytes: `{op:8, dst:8, left:8, right:8}`. 16-bit jump targets pack into `left|right` via `getAddr()`/`setAddr()`. Frame locals live at `mem[FP + (int8_t)offset]` — the offset fits in a signed byte, so functions are limited to ~28 locals. The compiler saves all function bodies with a leading `JMP` to skip over them; `main` is called explicitly at the end of `compile()`. Forward calls are patched after the full program is compiled.

**VM** (`vm.hpp/cpp`): Flat register file (256 `Value`s) and flat memory (`_mem`, 65536 `Value`s). Globals live at `_mem[0..globalSlots-1]`; the stack grows downward from address 10000. On `CALL`, the entire register file is saved into `CallFrame.savedRegs` and restored on `RETURN` (only the return-value register is overridden). This is necessary because the caller's live registers would otherwise be clobbered by the callee. `LOAD_OUTER`/`STORE_OUTER` walk up the call stack via `_callStack[size - hops].callerFp` to reach enclosing frames.

**Value** (`value.hpp`): `std::variant<std::monostate, double, std::string>` — none / num / str. `valStr()` formats doubles without a decimal point when the value is an integer.

**Bytecode files** (`.ngb`): Written/read by `writeBytecode()`/`readBytecode()` in `compiler.cpp`. Magic bytes `NGOB`, then counts (4 bytes each), line table, instructions, constant pool (8-byte doubles), length-prefixed strings, global slot count and names.

## Key constraints

- Every program must define `void fn main()` — the compiler errors without it.
- `print(x)` with a single argument automatically appends `"\n"`. Multi-arg print does not.
- The `//` operator is floor division; `%/` is float modulo (`fmod`); `%` is integer modulo.
- Jump addresses are 16-bit, so programs are limited to 65535 instructions.
- Frame sizes over ~127 bytes (32 locals) break the signed-byte ADDI encoding.
