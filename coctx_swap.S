
.globl coctx_swap
#if !defined( __APPLE__ )
.type  coctx_swap, @function
#endif
coctx_swap:

#if defined(__i386__) ;保存当前寄存器的值到第一个参数中
    movl 4(%esp), %eax ;这里ESP获取到的是对应图中old %EIP的地址 句话保存到reg[0] 
    movl %esp,  28(%eax) ;第八个值 这个位置本来是函数调用的返回地址 reg[7]存储栈帧 下面以此类推
    movl %ebp, 24(%eax)
    movl %esi, 20(%eax)
    movl %edi, 16(%eax)
    movl %edx, 12(%eax)
    movl %ecx, 8(%eax)
    movl %ebx, 4(%eax)
 

    movl 8(%esp), %eax ;把第二个参数推入eax 下面把第二个参数的值推到各个寄存器中
    movl 4(%eax), %ebx
    movl 8(%eax), %ecx
    movl 12(%eax), %edx  
    movl 16(%eax), %edi
    movl 20(%eax), %esi
    movl 24(%eax), %ebp
    movl 28(%eax), %esp

	ret

#elif defined(__x86_64__)
	leaq (%rsp),%rax
    movq %rax, 104(%rdi)
    movq %rbx, 96(%rdi)
    movq %rcx, 88(%rdi)
    movq %rdx, 80(%rdi)
	  movq 0(%rax), %rax
	  movq %rax, 72(%rdi) 
    movq %rsi, 64(%rdi)
	  movq %rdi, 56(%rdi)
    movq %rbp, 48(%rdi)
    movq %r8, 40(%rdi)
    movq %r9, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)
	  xorq %rax, %rax

    movq 48(%rsi), %rbp
    movq 104(%rsi), %rsp
    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 32(%rsi), %r9
    movq 40(%rsi), %r8
    movq 56(%rsi), %rdi
    movq 80(%rsi), %rdx
    movq 88(%rsi), %rcx
    movq 96(%rsi), %rbx
		leaq 8(%rsp), %rsp
		pushq 72(%rsi)

    movq 64(%rsi), %rsi
	ret
#endif
