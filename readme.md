# Mugen

Mugen is a microcode generator that converts a structured specification file into binary microcode images. These images can be flashed onto ROM chips for use in 8-bit computers, such as Ben Eater's breadboard computer. The tool allows hobbyists to define and customize control signals, instruction sets, and execution sequences for their own CPU designs.


## Installation

To build and install Mugen, run the following commands:

```sh
cd src
make
sudo make install
```

This will compile the source code and install the `mugen` binary into `/usr/local/bin`.

## Usage

Run Mugen with a specification file to generate the microcode image(s):

```sh
mugen input.mu microcode.bin
```

When more than 8 control signals have been defined (and therefore more than a single 8-bit ROM chip has to be used to store the microcode table), multiple files will be generated: microcode.bin.0, microcode.bin.1, etc.

### Printing Layout
The `--layout` or `-l` flag can be passed to Mugen if you want to see (or save for reference) the resulting memory layout. It will show what signals will be stored in which bit of every ROM chip and provide an overview of how each address-bit has been defined.

```sh
mugen input.mu microcode.bin --layout
```

## Specification File Format

A Mugen specification file (.mu) consists of the following sections: signals, opcodes, microcode, address and rom. Outside sections, only comments are permitted. Comments start with a `#` and end at the end of the line.

### ROM Configuration
Defines ROM parameters (number of words and bits per word). Currently only 8 bit ROM is supported. 

```
[rom] {
    8192x8
}
```
An optional third parameter can be added when using multiple rom chips.

```
[rom] {
    8192x8x3
}
```

### Address Breakdown
Specifies how the microcode is addressed. The values specify the number of bits used for each part of the address. 

```
[address] {
    cycle:  3
    opcode: 4
    flags:  2
}
```

The order in which these are declared determines their position in the address. For example, the declaration above leads to the following configuration (for a 8192 byte ROM):

``` 
Address Bit: 13 12 11 10 09 08 07 06 05 04 03 02 01 00 
              X  X  X  X  X  F  F  O  O  O  O  C  C  C
```

#### Segments
The address space may be segmented to allow for groups of 8 control signals to be stored in different segments of the same chip. The hardware must then be designed to sequentially load these signals from the different segments by enabling the corresponding segment bits. For example, when using 2 segment bits (4 segments), 24 signals can be stored on the same chip.

```
[address] {
    cycle:   3  # bits 0-2
    opcode:  4  # bits 3-6
    flags:   2  # bits 7-8
    segment: 2  # bits 9-10 are used to select the ROM segment
}
```

### Signals
Defines all control signals used in the microcode. At most 64 signals may be declared.

```
[signals] {
    HLT
    MI
    RI
    RO
    IO
    II
    #...
}
```
Each signal has to appear on a new line; the first signal will appear as the least significant bit in the resulting control signal configurations.

#### Signal Indices
Signals are grouped into chunks of 8 signals. The first chunk will be stored to the first chip, the second to the second chip and so on. When the chips have been segmented, sequential chunks are first stored in segment 0 of the corresponding ROM chips, then to segment 1 and so on. Given `n` available ROM chips, a chunk with index `c` will be stored in ROM `floor(c / n)`, segment `mod(c, n)`. 

### Opcodes
Defines the available opcodes and assigns their numerical values (in hex). Each opcode must be defined on its own line.

```
[opcodes] {
    LDA = 0x01   # these must be hexadecimal values
    ADD = 0x02
    OUT = 0x0e
    #...
}
```

### Microcode Definitions
Describes the control signals for each instruction cycle. Each line specificies the opcode, cycle and flag configuration followed by `->` and a list of control signals (which may be empty). Wildcards denoted `x` will be matched to any opcode, any cycle number within the specified range or either 0 or 1 in the case of the flags.

```
[microcode] {
    x:0:xx -> MI, CO
    x:1:xx -> RO, II, CE
		
    NOP:2:xx ->
    NOP:3:xx ->
    NOP:4:xx ->
		
    LDA:2:xx -> MI, IO
    LDA:3:xx -> RO, AI
    LDA:4:xx ->

    # ...
}
```

#### catch
It might be useful to fill all yet undefined addresses with some kind of error-signal to indicate that the computer ended up in some undefined state. This can be done using wildcards or the reserved `catch` keyword. In either case below, all remaining cells will be assigned the ERR and HLT signal.

```
[microcode] {
    # all previous rules
    
    catch -> ERR, HLT
    x:x:xxxx -> ERR, HLT   # this is equivalent
}
```

Only the catch rule is allowed to overlap with preceding rules. On every other rule an error will be raised when it is found to overlap with previously defined rules. Any normal rule following a catch-rule will always collide with the catch itself. Therefore the catch-rule should always come at the end of the microcode section.

```
[microcode] {
    # ...
    LDA:2:0x -> MI, IO
    LDA:2:01 -> R0, AI   # will collide with the rule above
    # ...
}
```

## Example
When Mugen is run on the example specification in the examples-folder of this repository, the following output is generated:

```
$ mugen bfcpu.mu bfcpu.bin --layout
Successfully generated 3 images from bfcpu.mu: 
  ROM 0 : bfcpu.bin.0
  ROM 1 : bfcpu.bin.1
  ROM 2 : bfcpu.bin.2

[ROM 0, Segment 0] {
  0: HLT
  1: RS0
  2: RS1
  3: RS2
  4: INC
  5: DEC
  6: DPR
  7: EN_SP
}

[ROM 1, Segment 0] {
  0: OE_RAM
  1: WE_RAM
  2: EN_IN
  3: EN_OUT
  4: VE
  5: AE
  6: LD_FB
  7: LD_FA
}

[ROM 2, Segment 0] {
  0: EN_IP
  1: LD_IP
  2: EN_D
  3: LD_D
  4: CR
  5: ERR
  6: UNUSED
  7: UNUSED
}

[Address Layout] {
  0: CYCLE 0
  1: CYCLE 1
  2: CYCLE 2
  3: OPCODE 0
  4: OPCODE 1
  5: OPCODE 2
  6: OPCODE 3
  7: FLAG 0
  8: FLAG 1
  9: FLAG 2
  10: FLAG 3
  11: UNUSED
  12: UNUSED
}
```