# The FSM prgramming language

FSM can stand for either the "Flying Sphaghetticode Monster", or for "Finite Statemachine Maker", whichever you prefer. The Monster doesn't mind if you don't believe in it.

This is a language that I am making up right now as a hobby project because I never wrote a real compiler in my life before, and I thought I'd better do it late than never.

# Hello, World
```fsm
fn main () {
    puts ("Hello, World!\n")
}
```

# Noteable features

## Operator chaining, or variadic operators

The comparison operators ('>', '<', '>=', '<=') can be arbitrarily chained to make the check you want:

```fsm
fn check_in_range (a: i64) {
    return 23 < a <= 42
}
```

The "or equal to" operator '|==' can extend the '==' operator, adding more true cases.
```fsm
fn do_things_for_some_numbers (val :i64) {
    if (val == 2 |== 3 |== 4 |== 9) { do_things () }
}
```

The "and also not equal to" operator '&!=' can extend the '!=' operator, excluding more true cases.
```fsm
fn do_things_except_for_some_numbers (val :i64) {
    if (val != 2 &!= 3 &!= 4 &!= 9) { do_things () }
}
```

## References with auto dereferencing and special reference binding operator
```fsm
let my_value = 42;
let my_other_value = 23;

let my_ref: i64&;

my_ref => my_value; // reference binding operator, reference now references 'my_value'

my_ref = 55; // Assign to the referenced value

let my_other_ref => my_other_value; // bind the reference in initialisation

// asignment
my_ref = my_other_ref; // 'my_value' is now 23 instead of 42

// vs. rebinding
my_ref => my_other_ref; // 'my_ref' now also references 'my_other_value'

my_2nd_order_ref => &my_ref; // 'my_2nd_order_ref' now references 'my_ref'
```

## Slices
The native string representation in fsm is the slice. A slice is a reference to beginning of the string, combined with the length of the string.

```fsm
struct {
    begin: T&;
    len:  i64; // Counts the number of Ts
}
```

```fsm
fn parse_integer (s: u8[]) {
    let i = 0;

    while (s.len && is_whitespace(s.begin)) s++;

    let neg = s.begin == '-';
    if (neg) s++;

    for ( ; s.len && is_numeric(s.begin); s++) {
        i = i * 10 + s.begin - '0';
    }
    if (neg) return 0 - i;
    return i;
}
```

Slices can also reference the data in arrays:
```fsm
fn put_some_numbers (my_slice: i64[]) {
    my_slice[0] = 42;
    my_slice[1] = 23;
    my_slice[2] = 1337;
}

fn main () {
    let my_array: i64[3];
    put_some_numbers(my_array);
}
```

# TODO:

- [x] Implement arrays
- [X] Implement slices
- [X] Implement reading files
- [X] Syntax highlighting
- [X] Import
- [X] Return 128 bit structs from functions
- [X] Char constants
- [X] Implement bit functions
- [ ] Write more documentation
- [ ] Implement null type for refs
- [ ] Implement enums
- [ ] Runtime bounds checks
- [ ] Global variables
- [ ] Implement function refs
- [ ] Memory management
- [ ] Make the compiler self hosting at some point



# NOT to do:
- [NOT] Impement C style pointers. FSM uses references, which are essentially pointers implementation wise, but they don't have any arithmetic operations. "Pointer arithmetic" is done in FSM by rebinding a reference to another element of an array or a slice, because that is the only context in which pointer arithmetic makes sense.
- [NOT] Implement C style bit operators (&, |, <<, >>, ^). FSM uses the 'bit...' set of builtins for performing these operations. That way it is clear when you are working with bits, and it reservs those nice operators for other things that are more important in a general programming language than messing with bits.
- [NOT] Implement C style 'break' and 'continue'. FSM will use the to be implemented 'goto' for those. The reasoning is that 'break' and 'continue' get confusing as soon as you have stacked loops, and you also can't even escape multiple levels. By doing this with 'goto' in FSM you can just goto wherever you want to change loop control flow. Goto labels are going to be allowed inside the post-action of 'for', so continuing loops can be implemented.


# Building the compiler
## dependencies
- x86_64 processor to be able to run the compiled executeables
- Linux operating system
- Make
- C compiler
- fasm (the flat assembler, Ubuntu has it as a package)

## building
The compiler is currently implemented in C. To build it just run "make" in the projects root folder.

## tests
To build and run the included fsm test programs, type "make test".

# Compiling a FSM program
Go to the project root directory and type:
```sh
./fsm my_program.fsm && fasm out.asm
```

The compiler will compile the fsm source to the assembly source 'out.asm'. Then fasm will assemble it, and you get 'out', a native Linux 64 bit elf executeable.

# Debugging the executeables

gdb can be used for debugging the generated executeables. There are no debugging symbols yet sadly, so this is more for debugging the bugs in the compiler than the bugs in the user program. For debugging your fsm program I would suggest to make use of the builtin 'print()' and 'puts()' functions, and the standard fsm library 'print_hex()' function.

```gdb
    set disassembly-flavor intel
    layout asm
    layout regs
    starti
    si
```

# Rule 110

Yes, it's Turing complete:

```fsm
fn print_bits(v: u64[]) {
    for (let i = 255; i >= 0; i--) {
        putc(if (bittest(v.begin, i)) '#' else ' ')
    }
    putc ('\n')
}

fn main () {

    let v: u64[2][4];

    for (let x = 0; x < 4; x++) {
        v[0][x] = 0;
        v[1][x] = 0;
    }

    v[0][0] = 1

    print_bits(v[0])

    let rule = 110;

    for (let j = 0; j < 255; j++) {
        let inp => v[j % 2];
        let outp => v[(j + 1) % 2];
        setbit(outp, 0, 1)

        for (let i = 0; i < 255; i++) {
            let input = bitand(bitshift(inp, i), 7)
            let bit = bittest(rule, input)
            setbit(outp, i+1, bit)
        }
        print_bits(outp)
    }
}
```