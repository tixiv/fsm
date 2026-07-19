# The FSM programming language

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
fn check_in_range (val: i64) : bool {
    return 23 < val <= 42
}
```

The "or equal to" operator '|==' can extend the '==' operator, adding more cases.
```fsm
fn do_things_for_specific_numbers (val :i64) {
    if (val == 2 |== 3 |== 4 |== 9) { do_things () }
}
```

The "and also not equal to" operator '&!=' can extend the '!=' operator, excluding more cases.
```fsm
fn do_things_except_for_specific_numbers (val :i64) {
    if (val != 2 &!= 3 &!= 4 &!= 9) { do_things () }
}
```

## For loops can have a result

Just add another ';' after the post action if you want this. It is usefull for usecases like this:

```fsm
fn main () {
    let contents = read_file("input");
    
    let pos: i32[1000];

    let num_pos = for (let x = 0; contents.len && x < 1000; x++; x) {
        pos[x] = parse_integer(take_number(contents));
    }

    puts ("Positions read: ") print (num_pos)
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
The native string representation in fsm is the slice. A slice is a reference to the beginning of the string, combined with the length of the string.

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
    if (neg) return -i;
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
- [X] Implement unary minus operator
- [X] Implement scopes inside functions
- [X] Implement enums
- [X] Formatted string printing
- [X] Zero initialize all variables
- [X] Make 'bool' 'true' and 'false' available
- [ ] Implement function refs
- [ ] Implement nested structs and unions
- [ ] Implement initializers for arrays and structs
- [ ] Write more documentation
- [ ] Implement null type for refs
- [ ] Runtime bounds checks
- [ ] Global variables
- [ ] Memory management
- [ ] Make the compiler self hosting at some point



# NOT to do:
- [NOT] Impement C style pointers. FSM uses references, which are essentially pointers implementation wise, but they don't have any arithmetic operations. "Pointer arithmetic" is done in FSM by rebinding a reference to another element of an array or a slice, because that is the only context in which pointer arithmetic makes sense.
- [NOT] Implement C style bit operators (&, |, <<, >>, ^). FSM uses the 'bit...' set of builtins for performing these operations. That way it is clear when you are working with bits, and it reserves those nice operators for other things that might be more important in a general programming language than messing with bits.
- [NOT] Implement C style 'break' and 'continue'. FSM will use the to be implemented 'goto' for those. The reasoning is that 'break' and 'continue' gets confusing as soon as you have stacked loops, and you also can't even escape multiple levels. By doing this with 'goto' in FSM you can just goto wherever you want to change loop control flow, and it's clear where you are going to. Goto labels are going to be allowed inside the post-action of 'for' loops, so 'continue' can be implemented that way.


# Building the compiler
## dependencies
- x86_64 processor to be able to run the compiled executeables
- Linux operating system
- Make
- C compiler
- fasm (the flat assembler, Ubuntu has it as a package)

## building
The compiler is currently implemented in C. To build it just run "make" in the project's root folder.

## tests
To build and run the included fsm test programs, type "make test".

# Compiling a FSM program
Go to the project root directory and type:
```sh
./fsm my_program.fsm && fasm out.asm
```

The compiler will compile the fsm source to an assembly source 'out.asm'. Then fasm will assemble it, and you get 'out', a native Linux 64 bit elf executeable.

# Debugging the executeables

gdb can be used for debugging the generated executeables. There are no debugging symbols yet sadly, so this is more for debugging the bugs in the compiler than the bugs in the user program. For debugging your fsm program I would suggest to make use of the builtin 'print()' and 'puts()' functions, and the standard fsm library 'print_hex()' function.

```gdb
    set disassembly-flavor intel
    layout asm
    layout regs
    starti
    si
```

# Examples

## Rule 110

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

## Fizz Buzz

```fsm
fn fizz(n) { if (n % 3 == 0) { puts("Fizz"); return 1 }; return 0 }
fn buzz(n) { if (n % 5 == 0) { puts("Buzz"); return 1 }; return 0 }
fn main () { for (let n = 0; n <= 100; n = n + 1) if (fizz(n) + buzz(n)) puts("\n") else print(n) }
```

## Recursive Fibonacci

```fsm
fn fib(n) {
    if (n == 1 |== 2) return 1;

    return fib(n-1) + fib(n-2);
}

fn main() {
    for (let n = 1; n <= 20; n++) print(fib(n));
}
```

## Tree Example

As I want to be self hosting one day, I tried out implementing a simple AST in fsm:

```fsm
struct TreeNode {
    type: i64;
    value: i64;
    left: TreeNode&;
    right: TreeNode&;
}

fn init_number (n: TreeNode&, v) {
    n.type = 0;
    n.value = v;
}

fn init_add (n: TreeNode&, left: TreeNode&, right: TreeNode&) {
    n.type = 1;
    n.left => left;
    n.right => right;
}

fn tree_visitor(n: TreeNode&) : i64 {
    if (n.type == 1) {
        let a = tree_visitor(n.left)
        let b = tree_visitor(n.right)
        puts("+\n=")
        print (a + b)
        return a + b
    } else {
        print (n.value)
        return n.value
    }
    return 0
}

fn main () {
    let a: TreeNode;
    init_number(&a, 42);    
    let b: TreeNode;
    init_number(&b, 2300);
    let c: TreeNode;
    init_number(&c, 13370000);
    let add_1: TreeNode;
    init_add(&add_1, &a, &b);
    let add_2: TreeNode;
    init_add(&add_2, &add_1, &c);

    tree_visitor(&add_2)
}
```
