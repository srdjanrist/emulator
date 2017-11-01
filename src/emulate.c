#include <stdio.h>
#include <stdlib.h>
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <semaphore.h>

#include "conf.h"
#include "definitions.h"

void jumpToInterrupt(int);

CPU_regs* cpu_regs = 0;
pthread_mutex_t reg_mutex;
pthread_t oneSecondTimer;
sem_t instrSem;
void push(int);

int getReg(int i) {
	int ret;
	pthread_mutex_lock(&reg_mutex);
	ret = cpu_regs->regs[i];
	pthread_mutex_unlock(&reg_mutex);
	return ret;
}

void setReg(int i, int val) {
	pthread_mutex_lock(&reg_mutex);
	cpu_regs->regs[i] = val;
	pthread_mutex_unlock(&reg_mutex);
}

void *oneSecondMethod(void*p) {

	while (1) {
		sleep(1);
		int psw = getReg(PSW);
		if (psw & (0x40000000)) {
			jumpToInterrupt(1);
		}
	}
	return 0;
}


pthread_t charListenerThread;
sem_t buffer_get, buffer_put;
sem_t regsem;
int n = 0;
int front = 0, tail = 0;
pthread_mutex_t mutex;
int charReceivedInt = 0;

void putChar(char c) {
	sem_wait(&buffer_put);
	n++;
	buffer[tail] = c;
	tail = (tail + 1) % BUFFER_CAPACITY;
	sem_post(&buffer_get);
}

char getChar() {
	sem_wait(&buffer_get);
	char ret = buffer[front];
	front = (front + 1) % BUFFER_CAPACITY;
	pthread_mutex_lock(&mutex);
	charReceivedInt = 0;
	if (n > 0)
		charReceivedInt = 1;

	pthread_mutex_unlock(&mutex);
	sem_post(&buffer_put);

	return ret;
}

void *charListenerMethod(void*p) {

	while (1) {
		char c = getchar();
		putChar(c);
	}
}


char buffer[BUFFER_CAPACITY];

int getWord(unsigned int p) {

	if (p > memorySize - 4) {
		printf("Error: Nedozvoljeno citanje sa adrese %x\n", p);
		abortEmulation();
	}
	int ret = 0;
	int i;
	for (i = 0; i < 4; i++)
		ret |= (memory[p + i] << ((3 - i) * 8));
	return ret;
}

int readInstruction(int p) {
	return getWord(p);
}

int isConditionMet(char condAndS) {
	int psw = getReg(PSW);
	char zeroFlag = psw & 0b0001;
	char overFlag = (psw & 0b0010) >> 1;
	char carryFlag = (psw & 0b0100) >> 2;
	char negFlag = (psw & 0b1000) >> 3;
	switch (condAndS >> 1) {

	case COND_EQ: {
		return zeroFlag == 1;
	}
	case COND_NE: {
		return zeroFlag == 0;
	}
	case COND_GT: {
		return (zeroFlag == 0 && negFlag == overFlag);
	}
	case COND_GE: {
		return negFlag == overFlag;
	}
	case COND_LT: {
		return negFlag != overFlag;
	}
	case COND_LE: {
		return (zeroFlag == 1 || negFlag != overFlag);
	}
	case COND_UNUSED: {
		return 1;
	}
	case COND_AL: {
		return 1;
	}
	}
}

void updateFlags(int result, int op1, int op2, int flags) {
	int psw = getReg(PSW);
	if (flags & FLG_Z)
		if (result == 0)
			psw |= FLG_Z;
		else
			psw &= ~FLG_Z;

	if (flags & FLG_O) {
		if ((op1 > 0 && op2 > 0 && result < 0) || (op1 < 0 && op2 < 0 && result > 0))
			psw |= FLG_O;
		else
			psw &= ~FLG_O;
	}
	if (flags & FLG_C) {
		if ((unsigned)result < (unsigned)op1 || (unsigned)result < (unsigned)op2)
			psw |= FLG_C;
		else
			psw &= ~FLG_C;
	}
	if (flags & FLG_N) {
		if (result < 0)
			psw |= FLG_N;
		else
			psw &= ~FLG_N;
	}


	setReg(PSW, psw);

}

void setWord(int val, int adr) {
	int i;
	for (i = 0; i < 4; i++)
		memory[adr + i] = (val >> ((3 - i) * 8)) & 0xFF;
}

void push(int val) {
	int sp = getReg(SP);
	sp -= 4;
	if (sp < memorySize - stackSize) {
		printf("Error: Stack overflow at address %X\n", sp);
		abortEmulation();
	}
	setReg(SP, sp);
	setWord(val, sp);
}

int pop() {
	int sp = getReg(SP);
	if (sp >= memorySize) {
		printf("Error: Stack underflow at address %X\n", sp);
	}
	int ret = getWord(sp);
	setReg(SP, sp + 4);
	return ret;
}

void execute() {
	do {
		sem_wait(&instrSem);
		int psw = getReg(PSW);
		if (psw & (~((~(unsigned)0) >> 1))) {

			pthread_mutex_lock(&mutex);
			int irq = charReceivedInt;
			pthread_mutex_unlock(&mutex);
			if (irq) {
				pthread_mutex_lock(&mutex);
				charReceivedInt = 0;
				pthread_mutex_unlock(&mutex);
				sem_post(&instrSem);
				jumpToInterrupt(3);
				continue;
			}

		}
		int pc = getReg(PC);
		int instr_word = readInstruction(pc);
		setReg(PC, pc + sizeof(WORD));

		if (!instr_word) {
			sem_post(&instrSem);
			return;
		}

		char condAndS = (instr_word >> 28) & 0xF;
		char opCode = (instr_word >> 24) & 0xF;

		if (!isConditionMet(condAndS)) {
			sem_post(&instrSem);
			continue;
		}

		switch (opCode) {
		case INT: {
			int intno = (instr_word >> 20) & 0xF;
			int psw = getReg(PSW);
			sem_post(&instrSem);
			jumpToInterrupt(intno);
			continue;
		}
		case ADD: case SUB: case MUL: case DIV: case CMP: {
			int dstReg = (instr_word >> 19) & 0x1F;
			int value = 0;
			if ((instr_word & (1 << 18)) == 0) {
				int srcReg = (instr_word >> 13) & 0x1F;
				value = getReg(srcReg);
			}
			else {
				value = instr_word & 0x3FFFF;
				value |= (instr_word & 0x20000 ? 0xFFFC : 0);
			}
			int result = getReg(dstReg);
			if (opCode == ADD) {
				result += value;
				if (condAndS & 1 == 1) {
					int arg = getReg(dstReg);
					updateFlags(result, arg, value, FLG_N | FLG_C | FLG_O | FLG_Z);	//NCOZ
				}
				setReg(dstReg, result);
			}
			else if (opCode == SUB) {
				result -= value;
				if (condAndS & 1 == 1) {
					int arg = getReg(dstReg);
					updateFlags(result, arg, -value, FLG_N | FLG_C | FLG_O | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == MUL) {
				result *= value;
				if (condAndS & 1 == 1) {
					int arg = getReg(dstReg);
					updateFlags(result, arg, value, FLG_N | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == DIV) {
				result /= value;
				if (condAndS & 1 == 1) {
					int arg = getReg(dstReg);
					updateFlags(result, arg, value, FLG_N | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == CMP) {
				result -= value;
				//	if (condAndS & 1 == 1) {
				int arg = getReg(dstReg);
				updateFlags(result, arg, value, FLG_N | FLG_C | FLG_O | FLG_Z);
				//}
			}

			break;
		}
		case AND: case OR: case NOT: case TEST: {
			int dstReg = (instr_word >> 19) & 0x1F;
			int srcReg = (instr_word >> 14) & 0x1F;
			int result = getReg(dstReg);
			if (opCode == AND) {
				result &= getReg(srcReg);
				if (condAndS & 1 == 1) {
					int arg = getReg(dstReg);
					int arg2 = getReg(srcReg);
					updateFlags(result, arg, arg2, FLG_N | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == OR) {
				result |= getReg(srcReg);
				if (condAndS & 1 == 1) {
					updateFlags(result, getReg(dstReg), getReg(srcReg), FLG_N | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == NOT) {
				result = ~getReg(srcReg);
				if (condAndS & 1 == 1) {
					updateFlags(result, getReg(dstReg), getReg(srcReg), FLG_N | FLG_Z);
				}
				setReg(dstReg, result);
			}
			else if (opCode == TEST) {
				result &= getReg(srcReg);
				if (condAndS & 1 == 1) {
					updateFlags(result, getReg(dstReg), getReg(srcReg), FLG_N | FLG_Z);
				}
			}

			break;
		}
		case LDRSTR: {
			int a = (instr_word >> 19) & 0x1F;
			int r = (instr_word >> 14) & 0x1F;
			int f = (instr_word >> 11) & 0b111;
			int l = (instr_word >> 10) & 1;
			int imm = (instr_word)& 0x3FF;

			if (f == 4)
				a += 4;
			else if (f == 5)
				a -= 4;
			if (l)
				setReg(r, memory[getReg(a) + imm]);
			else
				memory[getReg(a) + imm] = getReg(r);
			if (f == 2)
				a += 4;
			else if (f == 3)
				a -= 4;

			break;
		}
		case CALL: {
			int dst = (instr_word >> 19) & 0x1F;
			int imm = (instr_word)& 0x7FFFF;
			imm |= (imm & 0x40000 ? 0xFFF80000 : 0);
			setReg(LR, getReg(PC));
			setReg(PC, getReg(dst) + imm);

			break;
		}
		case INOUT: {
			int dst = (instr_word >> 20) & 0xF;
			int src = (instr_word >> 16) & 0xF;
			int io = (instr_word >> 15) & 1;

			if (io) {
				if (getReg(src) == 0x1000)
					setReg(dst, getChar());
			}
			else {
				if (getReg(dst) == 0x2000)
					putchar(getReg(src));
			}


			break;
		}
		case MOVSHRSHL: {
			int dst = (instr_word >> 19) & 0x1F;
			int src = (instr_word >> 14) & 0x1F;
			int imm = (instr_word >> 9) & 0x1F;
			int d = (instr_word >> 8) & 1;
			int value = getReg(src);
			if (d == 1)
				value <<= imm;
			else {
				if (imm >= 1) {
					value >>= 1;
					value &= 0x7FFFFFFF;
					value >>= (imm - 1);
				}
			}

			if (dst == PC && (condAndS & 1)) {
				setReg(PSW, pop());
			}
			setReg(dst, value);

			break;
		}
		case LDCHL: {
			int dst = (instr_word >> 20) & 0xF;
			int h = (instr_word >> 19) & 1;
			int c = (instr_word & 0xFFFF);

			if (h == 1) {
				setReg(dst, getReg(dst) | (c << 16));
			}
			else {
				setReg(dst, 0);
				setReg(dst, c);
			}
			break;
		}
		default: {
			sem_post(&instrSem);
			jumpToInterrupt(2);
			continue;
		}

		}
		sem_post(&instrSem);
	} while (1);

}

void jumpToInterrupt(int intNo) {
	if (intNo < 0 || intNo > 15) {
		jumpToInterrupt(2);
	}
	else {
		sem_wait(&instrSem);
		if (intNo != 0) {
			push(getReg(PSW));
			setReg(PSW, getReg(PSW) & ((~(unsigned)0 >> 1)));
		}
		setReg(LR, getReg(PC));
		setReg(PC, getWord(intNo * 4));
		sem_post(&instrSem);
	}
}

void start() {
	jumpToInterrupt(0);
	execute();
}

void emulate() {
	pthread_mutex_init(&reg_mutex, 0);
	cpu_regs = (CPU_regs*)calloc(1, sizeof(CPU_regs));
	int i;
	for (i = 0; i <= PSW; i++) {
		setReg(i, 0);
	}
	setReg(SP, memorySize);
	sem_init(&buffer_put, 0, BUFFER_CAPACITY);
	sem_init(&buffer_get, 0, 0);
	sem_init(&instrSem, 0, 1);
	pthread_mutex_init(&mutex, 0);

	pthread_create(&charListenerThread, 0, charListenerMethod, 0);
	pthread_create(&oneSecondTimer, 0, oneSecondMethod, 0);
	start();

	pthread_cancel(charListenerThread);
	pthread_cancel(oneSecondTimer);

	FILE *fdump = fopen("regDump", "w");
	if (fdump != 0){
		for (i = 0; i <= PSW; i++){
			fprintf (fdump, "%d %X\n", i, getReg(i));
		}
		fclose(fdump);
	}
}
