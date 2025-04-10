# This example shows the microcode specification for the Brainf*ck CPU.
# For more information on this project, see https://github.com/jorenheit/bfcpu.

[rom] { 8192 x 8 }

[address] {
  cycle:   3
  opcode:  4
  flags:   A, V, S, Z
  segment: 2  
}

[signals] {
  HLT
  RS0
  RS1
  RS2
  INC
  DEC
  DPR
  EN_SP
  OE_RAM
  WE_RAM
  EN_IN
  EN_OUT
  VE
  AE
  LD_FB
  LD_FA
  EN_IP
  LD_IP
  EN_D
  LD_D
  CR
  ERR
}

[opcodes] {
  NOP		= 0x00
  PLUS		= 0x01
  MINUS		= 0x02
  LEFT		= 0x03
  RIGHT		= 0x04
  IN_BUF	= 0x05
  IN_IM		= 0x06
  OUT		= 0x07
  LOOP_START	= 0x08
  LOOP_END	= 0x09
  INIT		= 0x0d
  HOME		= 0x0e
  HALT		= 0x0f
}

[microcode] {
  x:0:xxxx		-> LD_FB
  
  PLUS:1:x00x		-> INC, RS0, VE, LD_FA
  PLUS:2:x00x		-> INC, RS2, CR
  PLUS:1:x10x		-> LD_D, OE_RAM, LD_FA, CR
  PLUS:1:xx1x		-> INC, RS2, CR

  MINUS:1:x00x		-> DEC, RS0, VE, LD_FA
  MINUS:2:x00x		-> INC, RS2, CR
  MINUS:1:x10x		-> LD_D, OE_RAM, LD_FA, CR
  MINUS:1:xx1x		-> INC, RS2, CR

  LEFT:1:0x0x		-> DEC, RS1, AE, LD_FA
  LEFT:2:0x0x		-> INC, RS2, CR
  LEFT:1:1x0x		-> EN_D, WE_RAM, LD_FA, CR
  LEFT:1:xx1x		-> INC, RS2, CR

  RIGHT:1:0x0x		-> INC, RS1, AE, LD_FA
  RIGHT:2:0x0x		-> INC, RS2, CR
  RIGHT:1:1x0x		-> EN_D, WE_RAM, LD_FA, CR
  RIGHT:1:xx1x		-> INC, RS2, CR

  LOOP_START:1:x001	-> INC, RS0, RS2
  LOOP_START:2:x001	-> INC, RS2, CR
  LOOP_START:1:x000	-> INC, RS0, RS1
  LOOP_START:2:x000	-> WE_RAM, EN_SP, EN_IP
  LOOP_START:3:x000	-> INC, RS2, CR
  LOOP_START:1:x10x	-> LD_D, OE_RAM, LD_FA, CR
  LOOP_START:1:xx1x	-> INC, RS0, RS2
  LOOP_START:2:xx1x	-> INC, RS2, CR

  LOOP_END:1:x001	-> DEC, RS0, RS1
  LOOP_END:2:x001	-> INC, RS0, RS2, CR
  LOOP_END:1:x000	-> EN_SP, OE_RAM, LD_IP
  LOOP_END:2:x000	-> INC, RS2, CR
  LOOP_END:1:x10x	-> OE_RAM, LD_D, LD_FA, CR
  LOOP_END:1:xx1x	-> DEC, RS0, RS2
  LOOP_END:2:xx1x	-> INC, RS2, CR

  OUT:1:x00x		-> EN_OUT, EN_D, INC, RS2, CR
  OUT:1:x10x		-> OE_RAM, LD_D, LD_FA
  OUT:2:x10x		-> EN_OUT, EN_D, INC, RS2, CR
  OUT:1:xx1x		-> INC, RS2, CR

  IN_BUF:1:xx0x		-> EN_IN
  IN_BUF:2:xx0x		-> LD_D
  IN_BUF:3:xx0x		-> LD_FB
  IN_BUF:4:xx00		-> VE, LD_FA, INC, RS2, CR
  IN_BUF:4:0x01		-> CR
  IN_BUF:1:xx1x		-> INC, RS2, CR

  IN_IM:1:xx0x		-> EN_IN
  IN_IM:2:xx0x		-> LD_D, VE, LD_FA
  IN_IM:3:xx0x		-> INC, RS2, CR
  IN_IM:1:xx1x		-> INC, RS2, CR

  NOP:1:xxxx		-> INC, RS2, CR
  HALT:1:xxxx		-> HLT
  HALT:2:xxxx		-> INC, RS2, CR

  INIT:1:xxx1		-> EN_D, WE_RAM, INC, RS0, RS2
  INIT:2:xxx1		-> LD_FB, INC, RS1
  INIT:3:xx01		-> INC, RS2, CR
  INIT:3:xx11		-> CR

  HOME:1:xxxx		-> DPR, INC, RS2, CR

  catch		        -> ERR, HLT
}



