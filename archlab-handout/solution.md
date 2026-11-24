## Part A

代码思路相对简单，执行以下 `bash` 操作，得到 `examples.c` 的汇编代码

```bash
gcc -Og -S -fno-stack-protector -fno-asynchronous-unwind-tables ./examples.c -o ./examples.s
```

然后复制 `sum_list` 部分代码，到 `sum.ys` 文件中，根据 `Y86` 的语法规则进行修改：

1. addq 需要两步来进行计算
2. testq 需要进行平替
3. 需要处理好栈帧的地址，放置栈帧冲破 `0x0000`
4. Y86 的所有操作都是 8 字节
5. Y86 的语法规则不允许函数出现 `.L2` 的情况

### sum.ys

结合 `writeup` 的测试案例

```ys
# Sample linked list
    .align 8
ele1:
    .quad 0x00a
    .quad ele2
ele2:
    .quad 0x0b0
    .quad ele3
ele3:
    .quad 0xc00
    .quad 0
```

完整的代码如下：

```ys
.pos 0
    irmovq stack, %rsp    # 必须是第一条执行的指令！
    call main
    halt

main:
    irmovq ele1, %rdi       # 将链表头地址作为第一个参数传给 %rdi
    call sum_list
    ret

sum_list:
	irmovq	$0, %rax
	jmp	L2
L3:
    mrmovq  (%rdi), %rsi
    addq	%rsi, %rax
	mrmovq	8(%rdi), %rdi
L2:
	andq    %rdi, %rdi
	jne	L3
	ret
   
# Sample linked list
    .align 8
ele1:
    .quad 0x00a
    .quad ele2
ele2:
    .quad 0x0b0
    .quad ele3
ele3:
    .quad 0xc00
    .quad 0

# 放在代码和数据之后的安全高地址
    .pos 0x200
stack:
```

执行
```ys
./yas sum.ys && ./yis sum.yo
```

响应为：

```bash
Stopped in 26 steps at PC = 0x13.  Status 'HLT', CC Z=1 S=0 O=0
Changes to registers:
%rax:   0x0000000000000000      0x0000000000000cba
%rsp:   0x0000000000000000      0x0000000000000200
%rsi:   0x0000000000000000      0x0000000000000c00

Changes to memory:
0x01f0: 0x0000000000000000      0x0000000000000027
0x01f8: 0x0000000000000000      0x0000000000000013
```

### rsum.ys

处理逻辑也非简单，直接修改就能直接 pass 了。

```ys
.pos 0
    irmovq stack, %rsp    # 必须是第一条执行的指令！
    call main
    halt

main:
    irmovq ele1, %rdi       # 将链表头地址作为第一个参数传给 %rdi
    call rsum_list
    ret

rsum_list:
	andq %rdi, %rdi
	je	L6
	pushq	%rbx
	mrmovq	(%rdi), %rbx
	mrmovq	8(%rdi), %rdi
	call	rsum_list
	addq	%rbx, %rax
	popq	%rbx
	ret
L6:
	irmovq	$0, %rax
	ret

# Sample linked list
    .align 8
ele1:
    .quad 0x00a
    .quad ele2
ele2:
    .quad 0x0b0
    .quad ele3
ele3:
    .quad 0xc00
    .quad 0

# 放在代码和数据之后的安全高地址（例如 0x200）
    .pos 0x200
stack:
```

响应的结果是：

```bash
❯ ./yas rsum.ys && ./yis rsum.yo
Stopped in 37 steps at PC = 0x13.  Status 'HLT', CC Z=0 S=0 O=0
Changes to registers:
%rax:   0x0000000000000000      0x0000000000000cba
%rsp:   0x0000000000000000      0x0000000000000200

Changes to memory:
0x01c0: 0x0000000000000000      0x0000000000000052
0x01c8: 0x0000000000000000      0x00000000000000b0
0x01d0: 0x0000000000000000      0x0000000000000052
0x01d8: 0x0000000000000000      0x000000000000000a
0x01e0: 0x0000000000000000      0x0000000000000052
0x01f0: 0x0000000000000000      0x0000000000000027
0x01f8: 0x0000000000000000      0x0000000000000013
```


### copy.ys

```ys
.pos 0
    irmovq stack, %rsp    # 必须是第一条执行的指令！
    call main
    halt

main:
    irmovq src, %rdi       # 将链表头地址作为第一个参数传给 %rdi
    irmovq dest, %rsi
    irmovq $0x3, %rdx
    call copy_block
    ret

copy_block:
	irmovq	$0, %rcx
	jmp	L12
L13:
    irmovq  $8, %r8
	mrmovq	(%rdi), %rax
	rmmovq	%rax, (%rsi)
	xorq	%rax, %rcx
    irmovq  $1, %r9
	subq	%r9, %rdx
    addq    %r8, %rsi
    addq    %r8, %rdi
    # 这部分不同于链表，因为链表这部分是直接读取指定地址的内容，数据是直接读指
	# mrmovq	8(%rsi), %rsi
	# mrmovq	8(%rdi), %rdi
L12:
	andq %rdx, %rdx
	jg	L13
	rrmovq	%rcx, %rax
	ret

.align 8
# Source block
src:
    .quad 0x00a
    .quad 0x0b0
    .quad 0xc00
# Destination block
dest:
    .quad 0x111
    .quad 0x222
    .quad 0x333

# 放在代码和数据之后的安全高地址（例如 0x200）
    .pos 0x200
stack:
```

响应结果是

```bash
❯ ./yas copy.ys && ./yis copy.yo
Stopped in 44 steps at PC = 0x13.  Status 'HLT', CC Z=1 S=0 O=0
Changes to registers:
%rax:   0x0000000000000000      0x0000000000000cba
%rcx:   0x0000000000000000      0x0000000000000cba
%rsp:   0x0000000000000000      0x0000000000000200
%rsi:   0x0000000000000000      0x00000000000000c0
%rdi:   0x0000000000000000      0x00000000000000a8
%r8:    0x0000000000000000      0x0000000000000008
%r9:    0x0000000000000000      0x0000000000000001

Changes to memory:
0x00a8: 0x0000000000000111      0x000000000000000a
0x00b0: 0x0000000000000222      0x00000000000000b0
0x00b8: 0x0000000000000333      0x0000000000000c00
0x01f0: 0x0000000000000000      0x000000000000003b
0x01f8: 0x0000000000000000      0x0000000000000013
```

## Part B

### 环境配置

CS:APP Arch Lab 自带了带图形界面的 Y86-64 模拟器（基于 Tcl/Tk 编写）。现代 Linux 发行版（如 Ubuntu 20.04/22.04+）默认不再预装老旧的 Tcl/Tk 8.5，且高版本 GCC 默认开启了更严格的符号与声明检查，直接执行 make 往往会遇到大量编译或链接错误。

```bash
>  make clean; make psim VERSION=full
rm -f psim pipe-*.c *.o *.exe *~ 
# Building the pipe-full.hcl version of PIPE
../misc/hcl2c -n pipe-full.hcl < pipe-full.hcl > pipe-full.c
gcc -Wall -O2 -fcommon -isystem /usr/include/tcl8.5 -I../misc -DHAS_GUI -o psim psim.c pipe-full.c \
        ../misc/isa.c -L/usr/lib -ltk -ltcl -lm
psim.c:23:10: fatal error: tk.h: No such file or directory
   23 | #include <tk.h>
      |          ^~~~~~
compilation terminated.
make: *** [Makefile:44: psim] Error 1
```

1. 安装对应的依赖库

```bash
sudo apt-get update
sudo apt-get install -y tcl-dev tk-dev flex bison
```

2. 修改对应的 makefile 文件

```makefile
# TKINC=-isystem /usr/include/tcl8.5
TKINC=-isystem /usr/include/tcl8.6

# Modify these two lines to choose your compiler and compile time
# flags.

CC=gcc
# CFLAGS=-Wall -O2 -fcommon
CFLAGS=-Wall -O2 -fcommon -DUSE_INTERP_RESULT -Wl,--unresolved-symbols=ignore-in-object-files
```

3. 重新编译

```bash
make clean && make
```

### 解题思路

根据 `writeup`  提示，结合 `OPQ` 和 `IRMOVQ` 的 pipeline 过程和习题 4.3 的指令就可以完成 `IADD` 的指令了。

![alt text](<solution/屏幕截图 2026-10-10 155820.png>)

```txt
# IADD
#
# Fetch:
# 	icode: ifun <- M1[PC]
# 	rA : rB: M1[PC+1]
# 	valC <- M8[PC+2]
#
# Decode:
# 	valB <- R[rB]
#
# Execute:
# 	valE <- valB + valA
# 	set_cc
#
# Memory:
# PC:
# 	PC <- valP
```

根据文档给出的指令，依次执行即可：

```bash
make VERSION=full
/ssim -t ../y86-code/asumi.yo

# 使用基准测试程序重新测试您的解决方案
cd ../y86-code; make testssim

# 执行回归测试。一旦基准测试程序能够正确运行，就应该运行 ../ptest 目录下的大量回归测试。要测试除 iaddq 之外的所有内容:
cd ../ptest; make SIM=../seq/ssim

# 测试你实现的 iaddq：
cd ../ptest; make SIM=../seq/ssim TFLAGS=-i
```

### 完整代码

```hcl
# IADD
#
# Fetch:
# 	icode: ifun <- M1[PC]
# 	rA : rB: M1[PC+1]
# 	valC <- M8[PC+2]
#
# Decode:
# 	valB <- R[rB]
#
# Execute:
# 	valE <- valB + valA
# 	set_cc
#
# Memory:
# PC:
# 	PC <- valP

#/* $begin seq-all-hcl */
####################################################################
#  HCL Description of Control for Single Cycle Y86-64 Processor SEQ   #
#  Copyright (C) Randal E. Bryant, David R. O'Hallaron, 2010       #
####################################################################

## Your task is to implement the iaddq instruction
## The file contains a declaration of the icodes
## for iaddq (IIADDQ)
## Your job is to add the rest of the logic to make it work

####################################################################
#    C Include's.  Don't alter these                               #
####################################################################

quote '#include <stdio.h>'
quote '#include "isa.h"'
quote '#include "sim.h"'
quote 'int sim_main(int argc, char *argv[]);'
quote 'word_t gen_pc(){return 0;}'
quote 'int main(int argc, char *argv[])'
quote '  {plusmode=0;return sim_main(argc,argv);}'

####################################################################
#    Declarations.  Do not change/remove/delete any of these       #
####################################################################

##### Symbolic representation of Y86-64 Instruction Codes #############
wordsig INOP 	'I_NOP'
wordsig IHALT	'I_HALT'
wordsig IRRMOVQ	'I_RRMOVQ'
wordsig IIRMOVQ	'I_IRMOVQ'
wordsig IRMMOVQ	'I_RMMOVQ'
wordsig IMRMOVQ	'I_MRMOVQ'
wordsig IOPQ	'I_ALU'
wordsig IJXX	'I_JMP'
wordsig ICALL	'I_CALL'
wordsig IRET	'I_RET'
wordsig IPUSHQ	'I_PUSHQ'
wordsig IPOPQ	'I_POPQ'
# Instruction code for iaddq instruction
wordsig IIADDQ	'I_IADDQ'

##### Symbolic represenations of Y86-64 function codes                  #####
wordsig FNONE    'F_NONE'        # Default function code

##### Symbolic representation of Y86-64 Registers referenced explicitly #####
wordsig RRSP     'REG_RSP'    	# Stack Pointer
wordsig RNONE    'REG_NONE'   	# Special value indicating "no register"

##### ALU Functions referenced explicitly                            #####
wordsig ALUADD	'A_ADD'		# ALU should add its arguments

##### Possible instruction status values                             #####
wordsig SAOK	'STAT_AOK'	# Normal execution
wordsig SADR	'STAT_ADR'	# Invalid memory address
wordsig SINS	'STAT_INS'	# Invalid instruction
wordsig SHLT	'STAT_HLT'	# Halt instruction encountered

##### Signals that can be referenced by control logic ####################

##### Fetch stage inputs		#####
wordsig pc 'pc'				# Program counter
##### Fetch stage computations		#####
wordsig imem_icode 'imem_icode'		# icode field from instruction memory
wordsig imem_ifun  'imem_ifun' 		# ifun field from instruction memory
wordsig icode	  'icode'		# Instruction control code
wordsig ifun	  'ifun'		# Instruction function
wordsig rA	  'ra'			# rA field from instruction
wordsig rB	  'rb'			# rB field from instruction
wordsig valC	  'valc'		# Constant from instruction
wordsig valP	  'valp'		# Address of following instruction
boolsig imem_error 'imem_error'		# Error signal from instruction memory
boolsig instr_valid 'instr_valid'	# Is fetched instruction valid?

##### Decode stage computations		#####
wordsig valA	'vala'			# Value from register A port
wordsig valB	'valb'			# Value from register B port

##### Execute stage computations	#####
wordsig valE	'vale'			# Value computed by ALU
boolsig Cnd	'cond'			# Branch test

##### Memory stage computations		#####
wordsig valM	'valm'			# Value read from memory
boolsig dmem_error 'dmem_error'		# Error signal from data memory


####################################################################
#    Control Signal Definitions.                                   #
####################################################################

################ Fetch Stage     ###################################

# Determine instruction code
word icode = [
	imem_error: INOP;
	1: imem_icode;		# Default: get from instruction memory
];

# Determine instruction function
word ifun = [
	imem_error: FNONE;
	1: imem_ifun;		# Default: get from instruction memory
];

bool instr_valid = icode in 
	{ INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
	       IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IIADDQ };

# Does fetched instruction require a regid byte?
bool need_regids =
	icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ, 
		     IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ };

# Does fetched instruction require a constant word?
bool need_valC =
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL, IIADDQ };

################ Decode Stage    ###################################

## What register should be used as the A source?
word srcA = [
	icode in { IRRMOVQ, IRMMOVQ, IOPQ, IPUSHQ  } : rA;
	icode in { IPOPQ, IRET } : RRSP;
	1 : RNONE; # Don't need register
];

## What register should be used as the B source?
word srcB = [
	icode in { IOPQ, IRMMOVQ, IMRMOVQ, IIADDQ  } : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't need register
];

## What register should be used as the E destination?
word dstE = [
	icode in { IRRMOVQ } && Cnd : rB;
	icode in { IIRMOVQ, IOPQ, IIADDQ} : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't write any register
];

## What register should be used as the M destination?
word dstM = [
	icode in { IMRMOVQ, IPOPQ } : rA;
	1 : RNONE;  # Don't write any register
];

################ Execute Stage   ###################################

## Select input A to ALU
word aluA = [
	icode in { IRRMOVQ, IOPQ } : valA;
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ } : valC;
	icode in { ICALL, IPUSHQ } : -8;
	icode in { IRET, IPOPQ } : 8;
	# Other instructions don't need ALU
];

## Select input B to ALU
word aluB = [
	icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		      IPUSHQ, IRET, IPOPQ, IIADDQ } : valB;
	icode in { IRRMOVQ, IIRMOVQ } : 0;
	# Other instructions don't need ALU
];

## Set the ALU function
word alufun = [
	icode == IOPQ : ifun;
	1 : ALUADD;
];

## Should the condition codes be updated?
bool set_cc = icode in { IOPQ, IIADDQ };

################ Memory Stage    ###################################

## Set read control signal
bool mem_read = icode in { IMRMOVQ, IPOPQ, IRET };

## Set write control signal
bool mem_write = icode in { IRMMOVQ, IPUSHQ, ICALL };

## Select memory address
word mem_addr = [
	icode in { IRMMOVQ, IPUSHQ, ICALL, IMRMOVQ } : valE;
	icode in { IPOPQ, IRET } : valA;
	# Other instructions don't need address
];

## Select memory input data
word mem_data = [
	# Value from register
	icode in { IRMMOVQ, IPUSHQ } : valA;
	# Return PC
	icode == ICALL : valP;
	# Default: Don't write anything
];

## Determine instruction status
word Stat = [
	imem_error || dmem_error : SADR;
	!instr_valid: SINS;
	icode == IHALT : SHLT;
	1 : SAOK;
];

################ Program Counter Update ############################

## What address should instruction be fetched at

word new_pc = [
	# Call.  Use instruction constant
	icode == ICALL : valC;
	# Taken branch.  Use instruction constant
	icode == IJXX && Cnd : valC;
	# Completion of RET instruction.  Use value from stack
	icode == IRET : valM;
	# Default: Use incremented PC
	1 : valP;
];
#/* $end seq-all-hcl */

```

## Part C

所需要的指令

```bash
# 检查长度是否超过 1000 Byte 的限制
../misc/yas ncopy.ys && ./check-len.pl < ncopy.yo
# 检查正确性
./correctness.pl
# 修改 pipe-full.hcl 后，重新构建模拟器
make clean; make psim VERSION=full
# 修改 ncopy.ys 后，检查 CPE、本地跑分
make drivers && ./benchmark.pl
```

### 未进行处理
```
Average CPE     15.18
Score   0.0/60.0
```

### 加入 IADDQ

根据 Part B 我们可以修改 `pipe-full.hcl` 实现 `iaddq` 指令，然后替换原有的 add 指令进行简化，得到 CPE 为 12.70

```
Average CPE     12.70
Score   0.0/60.0

```

### 加入循环展开

根据 `write-up` 给出的提示，我们可以进行循环展开，然后进行进一步优化。

> 循环展开的思想就是通过空间换取时间，通过往不同的寄存器（或累加器）中分散加载与计算，从而打破数据依赖链，充分填满 CPU 的流水线与多个硬件执行单元。

```
Average CPE     8.65
Score   37.0/60.0
```

### 加入气泡处理

1. 在 `mrmovq` 和 `rmmovq` 之间存在有数据冒险，我们可以通过指令乱排的办法加入一个气泡来缓解这个问题。
2. 在 `andq` 和 `jle` 之间存在控制冒险

```ys
Loop:	
	mrmovq (%rdi), %r10	# read val from src...
	# 存在数据冒险
	rmmovq %r10, (%rsi)	# ...and store it to dst
	andq %r10, %r10		# val <= 0?
	# 存在控制冒险
	jle Npos		# if so, goto Npos:
	iaddq $1, %rax
	
Npos:	
	iaddq $-1, %rdx     # len--
	iaddq $8, %rdi		# src++
	iaddq $8, %rsi		# dst++
	andq %rdx,%rdx		# len > 0?
	jg Loop			# if so, goto Loop:
```

```
Average CPE     8.22
Score   45.7/60.0
```

###  完整代码


```ys
#/* $begin ncopy-ys */
##################################################################
# ncopy.ys - Copy a src block of len words to dst.
# Return the number of positive words (>0) contained in src.
#
# Include your name and ID here.
#
# Describe how and why you modified the baseline code.
#
##################################################################
# Do not modify this portion
# Function prologue.
# %rdi = src, %rsi = dst, %rdx = len
ncopy:

##################################################################
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	iaddq $-8, %rdx
	jl handler_remain_elements # if so, goto Done:

Loop8:
    # --- 阶段 1: 批量并发读取（前两个），打断单个寄存器停顿 ---
    mrmovq (%rdi), %r8          # read val0
    mrmovq 8(%rdi), %r9         # read val1

    rmmovq %r8, (%rsi)          # write val0
    andq %r8, %r8               # set CC for val0
    jle E1                      # val0 <= 0 跳过
    iaddq $1, %rax              # count++
E1:
    mrmovq 16(%rdi), %r10       # 提前预取 val2
    rmmovq %r9, 8(%rsi)        
    andq %r9, %r9              
    jle E2
    iaddq $1, %rax
E2:
    mrmovq 24(%rdi), %r11       
    rmmovq %r10, 16(%rsi)

    andq %r10, %r10
    jle E3
    iaddq $1, %rax
E3:
    mrmovq 32(%rdi), %r12       
    rmmovq %r11, 24(%rsi)

    andq %r11, %r11
    jle E4
    iaddq $1, %rax
E4:
    mrmovq 40(%rdi), %r13       
    rmmovq %r12, 32(%rsi)

    andq %r12, %r12
    jle E5
    iaddq $1, %rax
E5:
    mrmovq 48(%rdi), %r14       
    rmmovq %r13, 40(%rsi)

    andq %r13, %r13
    jle E6
    iaddq $1, %rax
E6:
    mrmovq 56(%rdi), %rbx       
    rmmovq %r14, 48(%rsi)
    andq %r14, %r14
    jle E7
    iaddq $1, %rax
E7:
    rmmovq %rbx, 56(%rsi)

    andq %rbx, %rbx
    jle NextLoop
    iaddq $1, %rax

NextLoop:
    iaddq $64, %rdi             # src += 8 (8*8)
    iaddq $64, %rsi             # dst += 8
    iaddq $-8, %rdx             # len -= 8
	jge Loop8

handler_remain_elements:	
	iaddq $8, %rdx
	jle Done # len = 0

Remain_loop:
	mrmovq (%rdi), %r10	# read val from src...
	iaddq $8, %rdi		# src++

	rmmovq %r10, (%rsi)	# ...and store it to dst

	andq %r10, %r10		# val <= 0?
	jle Npos		# if so, goto Npos:
	iaddq $1, %rax
	
Npos:	
	iaddq $8, %rsi		# dst++
	iaddq $-1, %rdx     # len--; set CC
	jg Remain_loop # if so, goto Loop:

##################################################################
# Do not modify the following section of code
# Function epilogue.
Done:
	ret
##################################################################
# Keep the following label at the end of your function
End:
#/* $end ncopy-ys */
```

### TODO

1. 余数判断上可以使用平衡二叉树进行进一步优化
2. 数据冒险部分还可以进行使用交错优化
3. 硬件层面上的优化