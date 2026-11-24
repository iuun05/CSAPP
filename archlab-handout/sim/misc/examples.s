	.file	"examples.c"
	.text
	.globl	sum_list
	.type	sum_list, @function
sum_list:
	endbr64
	movl	$0, %eax
	jmp	.L2
.L3:
	addq	(%rdi), %rax
	movq	8(%rdi), %rdi
.L2:
	testq	%rdi, %rdi
	jne	.L3
	ret
	.size	sum_list, .-sum_list
	.globl	rsum_list
	.type	rsum_list, @function

rsum_list:
	endbr64
	testq	%rdi, %rdi
	je	.L6
	pushq	%rbx
	movq	(%rdi), %rbx
	movq	8(%rdi), %rdi
	call	rsum_list
	addq	%rbx, %rax
	popq	%rbx
	ret
.L6:
	movl	$0, %eax
	ret

copy_block:
	endbr64
	movl	$0, %ecx
	jmp	.L12
.L13:
	movq	(%rdi), %rax
	movq	%rax, (%rsi)
	xorq	%rax, %rcx
	subq	$1, %rdx
	leaq	8(%rsi), %rsi
	leaq	8(%rdi), %rdi
.L12:
	testq	%rdx, %rdx
	jg	.L13
	movq	%rcx, %rax
	ret
	.size	copy_block, .-copy_block
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"%ld"
	.text
	.globl	main
	.type	main, @function
main:
	endbr64
	subq	$56, %rsp
	movq	$3072, 32(%rsp)
	movq	$0, 40(%rsp)
	movq	$176, 16(%rsp)
	leaq	32(%rsp), %rax
	movq	%rax, 24(%rsp)
	movq	$10, (%rsp)
	leaq	16(%rsp), %rax
	movq	%rax, 8(%rsp)
	movq	%rsp, %rdi
	call	sum_list
	movq	%rax, %rdx
	leaq	.LC0(%rip), %rsi
	movl	$1, %edi
	movl	$0, %eax
	call	__printf_chk@PLT
	movl	$0, %eax
	addq	$56, %rsp
	ret
	.size	main, .-main
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
