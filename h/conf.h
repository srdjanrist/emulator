#ifndef _CONF_H_
#define _CONF_H_

#define BUFFER_CAPACITY 64

extern char buffer[BUFFER_CAPACITY];

typedef int WORD;
typedef unsigned char byte;

extern void emulate();

extern byte *memory;
extern int memorySize;
extern int stackSize;

enum {
	INT, ADD, SUB, MUL, DIV,
	CMP, AND, OR, NOT, TEST,
	LDRSTR, CALL=12, INOUT,	MOVSHRSHL, LDCHL
};

enum {
	COND_EQ, COND_NE, COND_GT, COND_GE, COND_LT, COND_LE, COND_UNUSED, COND_AL
};

enum {
	FLG_Z = 1, FLG_O = 2, FLG_C = 4, FLG_N = 8
};

enum {
	PC = 16, LR, SP, PSW
};

typedef struct CPU_regs {
	WORD regs[20];
}CPU_regs;


#endif
