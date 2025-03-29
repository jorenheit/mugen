# Mugen

Mugen is a microcode generator that converts a structured specification file into binary microcode images. These images can be flashed onto ROM chips for use in 8-bit computers, such as Ben Eater's breadboard computer. The tool allows hobbyists to define and customize control signals, instruction sets, and execution sequences for their own CPU designs.


## Installation

To build and install Mugen, run the following commands:

```sh
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


## Specification File Format

A Mugen specification file (.mu) consists of the following sections: signals, opcodes, microcode, address and rom.

### Signals
Defines all control signals used in the microcode.

```
[signals] {
    HLT,
    MI,
    RI,
    RO,
    IO,
    II,
    ...
}
```
The first signal will appear as the least significant bit in the resulting control signal configurations.

### Opcodes
Assigns numerical values to each instruction.

```
[opcodes] {
    LDA  = 0x01
    ADD  = 0x02
    OUT  = 0x0e
    ...
}
```

### ROM Configuration
Defines ROM parameters (number of words and bits per word). Currently only 8 bit ROM is supported.

```
[rom] {
    8192x8
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

### Microcode Definitions
Describes the control signals for each instruction cycle. Each line specificies the opcode, cycle and flag configuration followed by `>` and a list of control signals (which may be empty). Wildcards denoted `x` will be matched to any opcode, any cycle number within the specified range or either 0 or 1 in the case of the flags.

```
[microcode] {
    x:0:xx > MI, CO
    x:1:xx > RO, II, CE
		
    NOP:2:xx >
    NOP:3:xx >
    NOP:4:xx >
		
    LDA:2:xx > MI, IO
    LDA:3:xx > RO, AI
    LDA:4:xx > 
}
```

## Contributing

Contributions and improvements are welcome! Feel free to submit issues and pull requests on GitHub.

## License

Mugen is released under the MIT License.

