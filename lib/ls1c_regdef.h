

#ifndef _ASM_REGDEF_H
#define _ASM_REGDEF_H


/*
 * Symbolic register names for 32 bit ABI
 */
	//30页有详细通用寄存器用途解释
#define zero    $0      /* wired zero */
#define AT      $1      /* assembler temp  - uppercase because of ".set at" *///汇编保留寄存器（不可做其他用途）
	
#define v0      $2      /* return value */ //子程序返回值
#define v1      $3
	
#define a0      $4      /* argument registers */ //子程序调用的前几个参数
#define a1      $5
#define a2      $6
#define a3      $7
	
#define t0      $8      /* caller saved */ //to--t9一个子程序可以不必要而自由使用的寄存机 temp
#define t1      $9
#define t2      $10
#define t3      $11
#define t4      $12
#define t5      $13
#define t6      $14
#define t7      $15
	
#define s0      $16     /* callee saved */ //s0--s7子程序变量 子程序写入时必须保存其值并在返回前需恢复
#define s1      $17
#define s2      $18
#define s3      $19
#define s4      $20
#define s5      $21
#define s6      $22
#define s7      $23
	
#define t8      $24     /* caller saved */ //to--t9一个子程序可以不必要而自由使用的寄存机 temp
#define t9      $25
	
#define jp      $25     /* PIC jump register */
	
#define k0      $26     /* kernel scratch */ //保留给中断或自陷处理程序用 其值可能在你眼皮底下改变 哇哦
#define k1      $27
	
#define gp      $28     /* global pointer */ //全局指针
#define sp      $29     /* stack pointer */  //堆栈指针
	
#define fp      $30     /* frame pointer */ //第九个寄存器变量 需要的子程序可以用来做帧(frame)指针
#define s8		$30		/* same like fp! */
	
#define ra      $31     /* return address */ //子程序返回地址

#endif /* _ASM_REGDEF_H */

