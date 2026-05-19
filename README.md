# nogo

A small scripting language I built for a university project. It has a full compiler pipeline (lexer → parser → bytecode compiler → register-based VM) and supports most things you'd expect from a scripting language.

## Building

Requires meson and ninja and a C++20 compiler.

```
meson setup build
ninja -C build
```

This produces a `nogo` binary in `build/`.

## Usage

```
nogo file.nogo              # compile and run a source file
nogo compile file.nogo      # compile to bytecode (outputs file.ngb)
nogo compile file.nogo out.ngb
nogo run file.ngb           # run pre-compiled bytecode
nogo run file.ngb --debug   # step through with the debugger
```

## Language

### Variables

```
let x = 10;
let name = "alice";
let nothing = none;

let a, b, c = 1, 2, 3;   // multi-assign

let global counter = 0;   // force global scope
let local tmp = 0;        // force local scope
```

Variables default to local inside functions, global at the top level.

### Types

Three types: `num` (64-bit float), `str`, `none`.

```
print(type(42));       // num
print(type("hi"));     // str
print(type(none));     // none
```

### Functions

```
fn add(a, b) {
    return a + b;
}

void fn greet(name) {
    print("hello " + name);
}

void fn main() {
    print(add(3, 4));
    greet("world");
}
```

Every program must have a `void fn main()`. It's called automatically.

### Control flow

```
if (x > 0) {
    print("positive");
} else {
    print("non-positive");
}

while (i < 10) {
    i += 1;
}

for (let i = 0; i < 10; i += 1) {
    if (i == 5) { continue; }
    print(i);
}

switch (x) {
    case 1:    { print("one");         break; }
    case 2, 3: { print("two or three"); break; }
    default:   { print("other");       }
}
```

### Operators

| Category   | Operators |
|------------|-----------|
| Arithmetic | `+  -  *  /  %  **  //  %/` |
| Bitwise    | `&  \|  ^  ~  <<  >>` |
| Comparison | `==  !=  <  >  <=  >=` |
| Logical    | `and  or  not` |
| Assign     | `+=  -=  *=  /=  %=  ^=` |
| Ternary    | `cond ? a : b` |

`**` is exponentiation, `//` is floor division, `%/` is float modulo.

### String operations

```
let s = "hello" + ", " + "world";
print(length(s));         // 13
print(s);

print(chr(65));           // A
print(ord("Z"));          // 90
print(hex(255));          // 0xff
print(bin(10));           // 0b1010
print(oct(8));            // 0o10
print(dec("0xff"));       // 255
```

### Math

```
print(sqrt(2));
print(sin(PI / 2));
print(log2(1024));
print(atan2(1, 1));
print(abs(-5));
print(floor(3.7));
print(round(2.5));
print(pow(2, 10));
```

Constants: `PI`, `E`, `INF`, `MAX`.

### Input

```
void fn main() {
    let name = input("enter your name: ");
    print("hello " + name);
}
```

### Imports

Split code across files with `import`:

```
import "utils.nogo"
import "math_helpers.nogo"

void fn main() {
    // use functions from imported files
}
```

Imports are resolved relative to the file they appear in. Circular imports are ignored.

## Bytecode format

Compiled `.ngb` files start with the magic bytes `NGOB` followed by instruction count, constant pool, string pool, and global variable metadata. The VM has 256 registers and 65536 memory slots.

## Debugger

Run with `--debug` to step through bytecode one instruction at a time:

```
nogo run file.ngb --debug
```

Commands inside the debugger:

| Key | Action |
|-----|--------|
| enter / `s` | step one instruction |
| `g` | run to end |
| `q` | quit |
| `r<N>` | print register N |
| `m<N>` | print memory slot N |
