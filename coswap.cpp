//
// Created by lizhaolong on 2020/6/8.
//

#include "coswap.h"

#include <string.h> // memset

// 协程栈转换部分直接用即可

namespace RocketCo {
    #define ESP 0
    #define EIP 1
    #define EAX 2
    #define ECX 3

    #define RSP 0
    #define RIP 1
    #define RBX 2
    #define RDI 3
    #define RSI 4

    #define RBP 5
    #define R12 6
    #define R13 7
    #define R14 8
    #define R15 9
    #define RDX 10
    #define RCX 11
    #define R8 12
    #define R9 13

    //----- --------
    // 32 bit
    // | regs[0]: ret |
    // | regs[1]: ebx |
    // | regs[2]: ecx |
    // | regs[3]: edx |
    // | regs[4]: edi |
    // | regs[5]: esi |
    // | regs[6]: ebp |
    // | regs[7]: eax |  = esp
    enum {
        kEIP = 0,
        kEBP = 6,
        kESP = 7,
    };

    //-------------
    // 64 bit
    // low | regs[0]: r15 |
    //    | regs[1]: r14 |
    //    | regs[2]: r13 |
    //    | regs[3]: r12 |
    //    | regs[4]: r9  |
    //    | regs[5]: r8  |
    //    | regs[6]: rbp |
    //    | regs[7]: rdi |
    //    | regs[8]: rsi |
    //    | regs[9]: ret |  //ret func addr
    //    | regs[10]: rdx |
    //    | regs[11]: rcx |
    //    | regs[12]: rbx |
    // hig | regs[13]: rsp |

    enum {
        kRDI = 7,
        kRSI = 8,
        kRETAddr = 9,
        kRSP = 13,
    };

    // 64 bit
    extern "C" {
    extern void coctx_swap(co_swap_t*, co_swap_t*) asm("coctx_swap");
    };

    #if defined(__i386__)
    int coctx_init(coctx_t* ctx) {
      memset(ctx, 0, sizeof(*ctx));
      return 0;
    }
    int coctx_make(coctx_t* ctx, coctx_pfn_t pfn, const void* s, const void* s1) {
      // make room for coctx_param
      // 此时sp其实就是esp指向的地方 其中ss_size感觉像是这个栈上目前剩余的空间,

      char* sp = ctx->ss_sp + ctx->ss_size - sizeof(coctx_param_t);
      //ctx->ss_sp 对应的空间是在堆上分配的，地址是从低到高的增长，而堆栈是往低地址方向增长的，
      //所以要使用这一块人为改变的栈帧区域，首先地址要调到最高位，即ss_sp + ss_size的位置

      sp = (char*)((unsigned long)sp & -16L);// 字节对齐

      coctx_param_t* param = (coctx_param_t*)sp;
      void** ret_addr = (void**)(sp - sizeof(void*) * 2); // 函数返回值
      // (sp - sizeof(void*) * 2) 这个指针存放着指向ret_addr的指针
      *ret_addr = (void*)pfn; // 新协程要执行的指令函数 但是放置的位置比较奇怪 没看懂
      param->s1 = s; //即将切换到的协程
      param->s2 = s1; // 切换出的线程

      memset(ctx->regs, 0, sizeof(ctx->regs));

      // ESP指针sp向下偏移2,因为除了ebp还有一个返回地址
      // 进入函数以后就会push ebp了
      ctx->regs[kESP] = (char*)(sp) - sizeof(void*) * 2;
      //函数调用，压入参数之后，还有一个返回地址要压入，所以还需要将sp往下移动4个字节，
      //32位汇编获取参数是通过EBP+4, EBP+8来分别获取第一个参数，第二个参数的，
      //这里减去4个字节是为了对齐这种约定,这里可以看到对齐以及参数还有4个字节的虚拟返回地址已经
      //占用了一定的栈空间，所以实际上供协程使用的栈空间是小于分配的空间。另外协程且走调用co_swap参数入栈也会占用空间，
      // KESP(7)在swap中是赋给esp的
      return 0;
    }
    #elif defined(__x86_64__)
    int co_swap_make(co_swap_t* cst, co_runningFunction pfn, const void* s, const void* s1) {
        // ss_sp指新的协程执行的栈空间
        char* sp = cst->ss_sp + cst->ss_size - sizeof(void*);
        sp = (char*)((unsigned long)sp & -16LL);

        memset(cst->regs, 0, sizeof(cst->regs));
        void** ret_addr = (void**)(sp);
        *ret_addr = (void*)pfn;

        cst->regs[kRSP] = sp;

        cst->regs[kRETAddr] = (char*)pfn;

        cst->regs[kRDI] = (char*)s;
        cst->regs[kRSI] = (char*)s1;
        return 0;
    }

    void co_swap_init(co_swap_t* cst){
        bzero(cst, sizeof *cst);
    }

    #endif

}