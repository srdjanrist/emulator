#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"
#include "bin_file_manip.h"
#include "conf.h"

FILE * fskript = 0;
int locationCounter = 0;
int startAddress = 0;
int startSet = 0;
byte *memory = 0;
int memorySize = 0;
int stackSize = 0;

int current_section = -1;
int sectionCounter = 0;
int totalSymbolCounter = 0;
int totalSectionCounter = 0;
int addedSections = 0;
int addedSymbols = 0;

int filesLoaded = 0;

symbol_entry** linkedSymbolTable = 0;
section_entry** linkedSectionTable = 0;
code** linkedCode = 0;


extern int bin_file_sections[] = {
	0x23FFFFFF,
	0x23CA00AA, 0x23DB00AA, 0x23EF00AA, 0x23FA00AA,
	0x23CA00FF, 0x23DB00FF, 0x23EF00FF, 0x23FA00FF
};

extern int checkPseudoExistance(char*, int*);
extern void addPseudoSymbol(char*, int);
int doesntExist(char *);

file_entry** all_files_data = 0;


void removeSpaces(char* source)
{
	char* i = source;
	char* j = source;
	while (*j != 0)
	{
		*i = *j++;
		if (*i != ' ')
			i++;
	}
	*i = 0;
}

void freeMemory() {
	int i;
	for (i = 0; i < addedSymbols; i++){
		free(linkedSymbolTable[i]);
	}
	if (linkedSymbolTable != 0)
		free (linkedSymbolTable);

	for (i = 0; i < addedSections; i++){
		free(linkedSectionTable[i]);
	}
	if (linkedSectionTable != 0)
		free (linkedSectionTable);

	for (i = 0; i < addedSections; i++){
		free(linkedCode[i]);
	}
	if (linkedCode != 0)
		free (linkedCode);	

	if (memory != 0)
		free (memory);
}

void finishEmulation() {
	freeMemory();
	return;
}

void abortEmulation() {
	freeMemory();
	/*	if (f != 0)
			fclose(f);*/
	exit(1);
}


int doesntExist(char * name) {
	int i;
	for (i = 0; i < addedSymbols; i++)
		if (strcmp(linkedSymbolTable[i]->name, name) == 0 && linkedSymbolTable[i]->binding == GLOBAL)
			return 0;
	return 1;

}

void updateRelocation(file_entry * fen, int index, int newindex, int newoffset) {
	int i;
	rel_entry **reltabela = fen->rel_table;
	int relnum = fen->relocationNumber;

	for (i = 0; i < relnum; i++) {
/*		if (reltabela[relnum]->value >= fen->symbolNumber) continue;
		symbol_entry *sent = fen->sym_table[reltabela[relnum]->value];
		if (sent && sent->binding == LOCAL) continue;*/


		if (reltabela[i]->value == index && ((reltabela[i]->fixed & 0b01) == 0)) {
			reltabela[i]->value = newindex;
			//		reltabela[i]->offset += newoffset;
			reltabela[i]->fixed |= 0b01;
		}
		if (reltabela[i]->section == index && ((reltabela[i]->fixed & 0b10) == 0)) {
			reltabela[i]->offset += newoffset;
			reltabela[i]->fixed |= 0b10;

		}
	}
	/*
	for (i = 0; i < filesLoaded; i++) {
		file_entry * fen2 = all_files_data[i];
		if (fen2 == fen)continue;
		rel_entry **reltabela = fen2->rel_table;
		int relnum = fen->relocationNumber;
		symbol_entry **symtabela = fen2->sym_table;

		int j;
		for (j = 0; j < relnum; j++) {
			symbol_entry *syment = symtabela[reltabela[j]->value];
			if (syment->binding == GLOBAL && (reltabela[j]->fixed & 0b01) == 0 &&
				strcmp(syment->name, linkedSymbolTable[newindex]->name ) == 0 ){
				reltabela[j]->value = newindex;
			}
			else if (syment->binding == GLOBAL && reltabela[j]->section == index &&
				(reltabela[j]->fixed & 0b10) == 0 && strcmp(syment->name, linkedSymbolTable[newindex]->name) == 0) {
				reltabela[j]->offset += newoffset;
				reltabela[j]->fixed |= 0b10;

			}
		}
	}*/
}

void addSectionsSymbols(int fileNum, int secNum, int newsectionindex) {
	file_entry * fen = all_files_data[fileNum];
	int symnum = fen->symbolNumber;
	int secnum = fen->sectionNumber;
	symbol_entry **symtable = fen->sym_table;

	int newoffset = linkedSymbolTable[addedSymbols - 1]->offset;

	int i;
	for (i = secnum; i < symnum; i++) {
		symbol_entry *entry = symtable[i];
		if (entry != 0 && entry->defined == 1 && entry->section == secNum) {
			int dummyInt = 0;
			if (doesntExist(entry->name) && !checkPseudoExistance(entry->name, &dummyInt)) {
				entry->offset += newoffset;
				entry->section = newsectionindex;
				entry->ord = addedSymbols + 1;
				linkedSymbolTable[addedSymbols] = entry;
				updateRelocation(fen, i, addedSymbols, newoffset);
				addedSymbols++;
				symtable[i] = 0;
			}
			else {
				printf("Error: Redefinicija simbola '%s'\n", entry->name);
				abortEmulation();
			}
		}
	}

}

void addSection(section_entry* sec_entry, int fileNum, int secNum) {
	int i;

	file_entry * fen = all_files_data[fileNum];

	char * temp = fen->sym_table[sec_entry->symbol_number]->name;

	for (i = 0; i < addedSymbols; i++) {
		if (strcmp(linkedSymbolTable[i]->name, temp) == 0) {
			printf("Error: Redefinicija sekcije '%s'\n", temp);
			abortEmulation();
		}
	}

	int newsecindex = addedSymbols;
	symbol_entry *sym_entry = fen->sym_table[sec_entry->symbol_number];
	sec_entry->added = 1;
	sec_entry->base = locationCounter;
	sym_entry->offset = locationCounter;
	sym_entry->section = newsecindex;
	fen->section_code[secNum]->section = newsecindex;

	linkedCode[addedSections] = fen->section_code[secNum];

	sym_entry->ord = newsecindex + 1;
	sec_entry->symbol_number = addedSymbols;
	locationCounter += sec_entry->length;

	linkedSymbolTable[addedSymbols] = sym_entry;
	linkedSectionTable[addedSections] = sec_entry;

	updateRelocation(fen, secNum, newsecindex, sec_entry->base);

	addedSymbols++;
	addedSections++;

	addSectionsSymbols(fileNum, secNum, newsecindex);
}

int getSymbolFromTable(char * symbol) {
	int i = 0;
	for (; i < addedSymbols; i++)
		if (strcmp(linkedSymbolTable[i]->name, symbol) == 0) {
			return i;
		}
	return -1;
}

int isPowerOfTwo(int n) {
	int ret = 0;

	while (n != 0) {
		if ((n & 1) == 0) {
			n >>= 1;
			continue;
		}
		if ((n & 1) && (ret == 0)) {
			ret = 1;
			n >>= 1;
		}
		else
			return 0;
	}
	return ret;
}

int processAligns(char * expr) {
	int ret = 0;
	int sign = 0;

	char * align = strstr(expr, "ALIGN");
	while (align) {
		char * start = align;
		if (align > expr) {
			start = align - 1;
			if (*(align - 1) == '-')
				sign = 1;
			else if (*(align - 1) == '+')
				sign = 0;
		}
		else
			sign = 0;

		align = strchr(align, '(') + 1;
		if (align == 0) {
			printf("Error: Greska kod align u skript fajlu\n");
			abortEmulation();
		}

		int firstarg = 0;

		while (*align != ',') {
			if (*align == '.') {
				firstarg = locationCounter;
			}
			else if (*align >= '0' && *align <= '9')
				firstarg = firstarg * 10 + (*align - '0');
			else {
				printf("Error: Greska u argumentima kod align u skript fajlu\n");
				abortEmulation();
			}
			align++;
		}

		int secondarg = 0;
		align++;
		while (*align != ')') {
			if (*align >= '0' && *align <= '9')
				secondarg = secondarg * 10 + (*align - '0');
			else {
				printf("Error: Greska u argumentima kod align u skript fajlu\n");
				abortEmulation();
			}
			align++;
		}

		if (isPowerOfTwo(secondarg)) {
			int inverted = secondarg - 1;
			firstarg += ((~(firstarg & inverted) + 1) & inverted);
		}
		else {
			while (firstarg % secondarg != 0) {
				firstarg++;
			}
		}

		if (sign)
			ret -= firstarg;
		else
			ret += firstarg;

		for (; start <= align; start++)
			*start = ' ';

		align = strstr(expr, "ALIGN");
	}
	removeSpaces(expr);
	return ret;

}

int isNumber(char * str) {
	while (*str) {
		if (*str <'0' || *str>'9')
			return 0;
		str++;
	}
	return 1;
}

int processExpression(char * strexpr) {
	char label[LABEL_MAX_LENGTH];
	char sign = 0;
	char oldSign = 0;
	int value = 0;

	int alignVal = processAligns(strexpr);
	value += alignVal;

	//	printf("%s\n", strexpr);

	while (*strexpr) {
		int i = 0;
		while (*strexpr && *strexpr != '+' && *strexpr != '-') {
			label[i++] = *strexpr;
			strexpr++;
		}
		if (*strexpr == '-')
			sign = 1;
		else if (*strexpr == '+')
			sign = 0;

		if (*strexpr != 0)
			strexpr++;

		label[i] = 0;
		if (i == 0) {
			oldSign = sign;
			continue;
		}

		int addValue = 0;

		if (*label == '.') {
			addValue = locationCounter;
		}
		else if (isNumber(label)) {
			addValue = atoi(label);
		}
		else {
			int pseudoVal;
			if (checkPseudoExistance(label, &pseudoVal)) {
				addValue = pseudoVal;
			}
			else {
				int labValue = getSymbolFromTable(label);
				if (labValue == -1 || linkedSymbolTable[labValue]->defined == 0) {
					printf("Error: Nedefinisan simbol '%s' u direktivi ALIGN\n", label);
					abortEmulation();
				}
				addValue = linkedSymbolTable[labValue]->offset;
			}
		}

		if (oldSign == 0)
			value += addValue;
		else
			value -= addValue;

		oldSign = sign;

		/*if (sym_table[labValue]->binding == LOCAL) {
			value += (sym_table[labValue]->offset);
			addRelocation(locationCounter, R_386_32, REL_W, sym_table[labValue]->section);
			//	printf("%d\t R_386_32\t%d\t%c\nValue in instr: %d\n", locationCounter, section_table[sym_table[labValue]->section]->symbol_number, oldSign ? '-' : '+', value);
		}*/
		/*else if (sym_table[labValue]->binding == GLOBAL) {
			addRelocation(locationCounter, R_386_32, oldSign ? REL_NW : REL_W, labValue);
			//	printf("%d\t R_386_32\t%d\t%c\nValue in instr: %d\n", locationCounter, labValue, oldSign ? '-' : '+', value);
		}*/

	}

	return value;
}

void addNewSymbol(char * name, char * value) {

	int i = 0;
	for (; i < addedSymbols; i++) {
		if (strcmp(name, linkedSymbolTable[i]->name) == 0) {
			printf("Error: Redefinicija simbola '%s'. Simbol vec postoji u tabeli globalnih simbola\n", name);
			abortEmulation();
		}
	}
	int dummyint;
	if (checkPseudoExistance(name, &dummyint) == 1) {
		printf("Error: Redefinicija simbola '%s'. Simbol je vec definisan u skriptu\n", name);
		abortEmulation();
	}

	addPseudoSymbol(name, processExpression(value));

}

void processAlloc(char * readLine) {
	int i;
	removeSpaces(readLine);

	char * token = strtok(readLine, "=");
	if (*token != '.') {
		addNewSymbol(token, strtok(0, "="));
	}
	else {
		int newLC = processExpression(strtok(0, "="));
		if (newLC >= locationCounter)
			locationCounter = newLC;
		else {
			printf("Error: Losa vrednost location countera navedena u skript fajlu\n");
			abortEmulation();
		}
	}

}

section_entry* findSectionInFile(char * readLine, int* fileNum, int* secNum) {
	int i;
	for (i = 0; i < filesLoaded; i++) {
		section_entry** tabela = all_files_data[i]->section_table;
		symbol_entry** tabelasimbola = all_files_data[i]->sym_table;

		int sectionNumber = all_files_data[i]->sectionNumber;
		int j;
		for (j = 0; j < sectionNumber; j++) {
			if (strcmp(readLine, tabelasimbola[j]->name) == 0) {
				*fileNum = i;
				*secNum = j;
				return all_files_data[*fileNum]->section_table[*secNum];
				break;
			}
		}
	}
	*fileNum = -1;
	*secNum = -1;
	return 0;
}

section_entry* findNextSectionToAdd(int* fileNum, int* secNum) {
	int i;
	section_entry* ret = 0;
	char * curName = 0;
	for (i = 0; i < filesLoaded; i++) {
		int j;
		file_entry* fent = all_files_data[i];
		for (j = 0; j < fent->sectionNumber; j++) {
			if (fent->section_table[j]->added == 0) {
				char * name = fent->sym_table[fent->section_table[j]->symbol_number]->name;
				if (ret == 0 || strcmp(name, curName) < 0) {
					curName = name;
					ret = fent->section_table[j];
					*fileNum = i;
					*secNum = j;
				}
			}
		}
	}
	if (ret == 0)
		*fileNum = -1;
	return ret;
}

void fixRelocations(file_entry* fen, char * name) {
	int symnum = 0;
	int i;
	int relnum = fen->relocationNumber;
	rel_entry ** reltabela = fen->rel_table;
	for (i = 0; i < addedSymbols; i++) {
		if (strcmp(name, linkedSymbolTable[i]->name) == 0) {
			symnum = i;
			int j;
			for (j = 0; j < relnum; j++) {
				rel_entry *relent = reltabela[j];
				if (relent) {
					if (fen->sym_table[relent->value] != 0) {
						char * relval = fen->sym_table[relent->value]->name;
						if (strcmp(relval, name) == 0) {
							reltabela[j]->value = symnum;
						}
					}
				}
			}
		}
	}

}

void checkForUnresolvedSymbols() {
	int i;
	for (i = 0; i < filesLoaded; i++) {
		symbol_entry** symtable = all_files_data[i]->sym_table;
		int symnum = all_files_data[i]->symbolNumber;
		int secnum = all_files_data[i]->sectionNumber;
		int j;
		for (j = secnum; j < symnum; j++) {
			if (symtable[j]) {
				if (symtable[j]->binding == GLOBAL && symtable[j]->defined == 0) {
					if (doesntExist(symtable[j]->name)) {
						printf("Error: Unresolved symbol '%s'\n", symtable[j]->name);
						abortEmulation();
					}
					else {
						fixRelocations(all_files_data[i], symtable[j]->name);
					}
				}
			}
		}
	}
}

void addPseudoSymbols() {
	while (isEmpty() == 0) {
		symbol_entry* temp = removePseudoSymbol();
		linkedSymbolTable[addedSymbols++] = temp;
		temp->ord = addedSymbols;
		int i;
		for (i = 0; i < addedSections; i++) {
			if (temp->offset - linkedSectionTable[i]->base < linkedSectionTable[i]->length) {
				temp->section = linkedSectionTable[i]->symbol_number;
				break;
			}
		}
		/*if (i == addedSections) {
			printf("Error: Simbol '%s' iz skript fajla je van svih sekcija\n", temp->name);
			abortEmulation();
		}*/

	}
}

void loadCode() {
	section_entry* secent = linkedSectionTable[addedSections - 1];
	memorySize = secent->base + secent->length;
	memorySize = memorySize;
	stackSize = 0x80 * (memorySize / 0x400 + 1);
	memorySize += stackSize;
	memory = (byte*)calloc(memorySize, sizeof(byte));

	int i;
	for (i = 0; i < addedSections; i++) {
		section_entry * secent = linkedSectionTable[i];
		int base = secent->base;
		int len = secent->length;
		code * code = linkedCode[i];
		char * memcode = code->code;
		memcpy(memory + base, memcode, len);
		free(memcode);
		free(code);
	}

}

void resolveRelocations()

{
	int i;
	for (i = 0; i < filesLoaded; i++) {
		file_entry * fen = all_files_data[i];
		rel_entry ** rtab = fen->rel_table;
		int rnum = fen->relocationNumber;

		int j;
		for (j = 0; j < rnum; j++) {
			rel_entry * ren = rtab[j];
			if (ren != 0 && ren->fixed) {
				int sign = 0;
				int adrToChange = ren->offset;
				int curVal = 0;
				switch (ren->bytes) {
				case REL_W: case REL_NW: {
					curVal |= memory[adrToChange] << 24;
					curVal |= memory[adrToChange + 1] << 16;
					curVal |= memory[adrToChange + 2] << 8;
					curVal |= memory[adrToChange + 3];
					if (ren->bytes == REL_NW)
						sign = 1;
					break;
				}
				case REL_HW0: case REL_HW1: {
					curVal |= memory[adrToChange] << 8;
					curVal |= memory[adrToChange + 1];
					break;
				}
				case REL_B0: case REL_B1: case REL_B2: case REL_B3: {
					curVal |= memory[adrToChange];
					break;
				}

				}

				int valToAdd = 0;
				valToAdd = linkedSymbolTable[ren->value]->offset;

				if (ren->type == R_386_32) {
					if (sign)
						curVal -= valToAdd;
					else
						curVal += valToAdd;

					switch (ren->bytes) {
					case REL_W: case REL_NW: {
						memory[adrToChange] = ((curVal >> 24) & 0xFF);
						memory[adrToChange + 1] = ((curVal >> 16) & 0xFF);
						memory[adrToChange + 2] = ((curVal >> 8) & 0x0FF);
						memory[adrToChange + 3] = (curVal & 0xFF);

						break;
					}
					case REL_HW0: case REL_HW1: {
						if (ren->bytes == REL_HW0) {
							memory[adrToChange] = ((curVal >> 24) & 0xFF);
							memory[adrToChange + 1] = ((curVal >> 16) & 0xFF);
						}
						else {
							memory[adrToChange] = ((curVal >> 8) & 0xFF);
							memory[adrToChange + 1] = (curVal & 0xFF);
						}
						break;
					}
					case REL_B0: case REL_B1: case REL_B2: case REL_B3: {
						memory[adrToChange] = (curVal >> ((REL_B3 - ren->bytes) * 8) & 0xFF);
						break;
					}

					}
				}
			}
		}
	}
}

void processScriptFile() {
	char readLine[LINE_MAX_LENGTH];
	int lineLen;

	linkedSectionTable = (section_entry**)calloc(totalSectionCounter, sizeof(section_entry*));
	linkedCode = (code**)calloc(totalSectionCounter, sizeof(code*));
	linkedSymbolTable = (symbol_entry**)calloc(totalSymbolCounter + 64, sizeof(symbol_entry*));


	while (1) {
		readLine[0] = 0;
		if (fgets(readLine, LINE_MAX_LENGTH, fskript) == 0) break;
		lineLen = strlen(readLine);
		if (readLine[lineLen - 1] == '\n')
			readLine[lineLen - 1] = '\0';

		if (lineLen == 1) continue;

		if (strchr(readLine, '=')) {
			processAlloc(readLine);
		}
		else {
			int fileNum, secNum;
			section_entry* sec_entry = findSectionInFile(readLine, &fileNum, &secNum);
			if (sec_entry == 0) {
				printf("Error: Nepostojeca sekcija '%s' navedena u skriptu\n", readLine);
				abortEmulation();
			}
			if (sec_entry->added == 1) {
				printf("Error: Sekcija '%s' je vec navedena u skriptu\n", readLine);
				abortEmulation();
			}
			addSection(sec_entry, fileNum, secNum);
		}
	}
	while (addedSections < totalSectionCounter) {
		int fileNum, secNum;
		section_entry* sec_entry = findNextSectionToAdd(&fileNum, &secNum);
		addSection(sec_entry, fileNum, secNum);
	}

	addPseudoSymbols();
	checkForUnresolvedSymbols();
	loadCode();
}

file_entry* loadFile(FILE * f, char* name) {
	file_entry* temp_file_entry = (file_entry*)calloc(1, sizeof(file_entry));

	int readInt;

	fread(&readInt, sizeof(int), 1, f);
	if (readInt != bin_file_sections[SYMBOL_TABLE]) {
		printf("Error: Ulazni fajl '%s' ima gresku\n", name);
		free(temp_file_entry);
		abortEmulation();
	}
	
	section_entry** section_table = 0;
	symbol_entry** sym_table = 0;
	rel_entry** relocation_table = 0;
	code** code_list = 0;

	fread(&readInt, sizeof(int), 1, f);			//procitan broj sekcija
	temp_file_entry->sectionNumber = readInt;
	totalSectionCounter += readInt;
	section_table = (section_entry**)calloc(temp_file_entry->sectionNumber, sizeof(section_entry*));
	temp_file_entry->section_code = (code**)calloc(temp_file_entry->sectionNumber, sizeof(code*));

	fread(&readInt, sizeof(int), 1, f);			//procitan broj simbola
	temp_file_entry->symbolNumber = readInt;
	totalSymbolCounter += readInt;
	sym_table = (symbol_entry**)calloc(temp_file_entry->symbolNumber, sizeof(symbol_entry*));


	fread(&readInt, sizeof(int), 1, f);			//procitan broj relokacija
	temp_file_entry->relocationNumber = readInt;
	if (readInt > 0) {
		relocation_table = (rel_entry**)calloc(temp_file_entry->relocationNumber, sizeof(rel_entry*));
	}

	int i;
	for (i = 0; i < temp_file_entry->sectionNumber; i++) {
		section_entry* temp_entry = (section_entry*)calloc(1, sizeof(section_entry));
		fread(temp_entry, sizeof(section_entry) - sizeof(int), 1, f);
		temp_entry->symbol_number--;
		temp_entry->added = 0;
		section_table[i] = temp_entry;
	}
	for (i = 0; i < temp_file_entry->symbolNumber; i++) {
		symbol_entry* temp_entry = (symbol_entry*)calloc(1, sizeof(symbol_entry));
		fread(temp_entry, sizeof(symbol_entry), 1, f);
		temp_entry->section--;
		sym_table[i] = temp_entry;
	}

	fread(&readInt, sizeof(int), 1, f);
	if (readInt != bin_file_sections[SYMBOL_TABLE + 4]) {
		printf("Error: Ulazni fajl '%s' ima gresku\n", name);
		abortEmulation();
	}

	if (temp_file_entry->relocationNumber > 0) {
		fread(&readInt, sizeof(int), 1, f);
		if (readInt != bin_file_sections[RELOCATION_TABLE]) {
			printf("Error: Ulazni fajl '%s' ima gresku\n", name);
			abortEmulation();
		}
		for (i = 0; i < temp_file_entry->relocationNumber; i++) {
			rel_entry* temp_entry = (rel_entry*)calloc(1, sizeof(rel_entry));
			fread(temp_entry, sizeof(rel_entry) - sizeof(int), 1, f);
			temp_entry->section--;
			temp_entry->value--;
			relocation_table[i] = temp_entry;
		}
		fread(&readInt, sizeof(int), 1, f);
		if (readInt != bin_file_sections[RELOCATION_TABLE + 4]) {
			printf("Error: Ulazni fajl '%s' ima gresku\n", name);
			abortEmulation();
		}
	}

	fread(&readInt, sizeof(int), 1, f);
	while (readInt != bin_file_sections[BIN_EOF]) {

		if (readInt != bin_file_sections[NEW_SECTION]) {
			printf("Error: Ulazni fajl '%s' ima gresku\n", name);
			abortEmulation();
		}

		fread(&readInt, sizeof(int), 1, f);
		int section = readInt - 1;
		fread(&readInt, sizeof(int), 1, f);
		int seclen = readInt;


		byte * code_temp = (byte*)calloc(seclen, sizeof(byte));
		code * codeentry = (code*)calloc(1, sizeof(code));
		codeentry->code = (char*)code_temp;
		codeentry->section = section;

		fread(code_temp, sizeof(byte), seclen, f);
		temp_file_entry->section_code[section] = codeentry;

		fread(&readInt, sizeof(int), 1, f);
	}

	temp_file_entry->section_table = section_table;
	temp_file_entry->sym_table = sym_table;
	temp_file_entry->rel_table = relocation_table;


	return temp_file_entry;
}

void loadAllFiles(int n, char** names) {
	all_files_data = (file_entry**)calloc(n, sizeof(file_entry*));

	int i;
	for (i = 0; i < n; i++) {
		FILE *f = fopen(names[i], "rb");
		if (f == 0) {
			printf("Error: Ne postoji ulazni fajl\n");
			abortEmulation();
		}
		file_entry* temp_file_entry = loadFile(f, names[i]);
		all_files_data[filesLoaded] = temp_file_entry;
		filesLoaded++;
		fclose(f);
	}
}

int main(int argv, char* argc[]) {

	if (argv < 3) {
		printf("Error: Potrebno je navesti skript fajl i jedan ili vise predmetnih programa\n");
		abortEmulation();
	}

	fskript = fopen(argc[1], "r");
	if (fskript == 0) {
		printf("Error: Skript fajl ne moze biti otvoren\n");
		abortEmulation();
	}

	loadAllFiles(argv - 2, argc + 2);
	/*printf("**************PRIJE SREDIVANJA RELOKACIJA**************\n");
	int i;
	for (i = 0; i < filesLoaded; i++) {
		printf("File %s\n", argc[i + 2]);
		rel_entry **reltabela = all_files_data[i]->rel_table;
		int relnum = all_files_data[i]->relocationNumber;

		for (i = 0; i < relnum; i++)
			printf("%d\t%d\t%d\t%d\t%d\n", reltabela[i]->section, reltabela[i]->offset, reltabela[i]->type, reltabela[i]->bytes,
				reltabela[i]->value);
	}*/

	processScriptFile();
	resolveRelocations();
	emulate();


	/*printf("************** NAKON UREDJIVANJA KODA **************\n\n");
	int i;
	int j = 0;
	for (i = 0; i < memorySize; i++) {
		char c = memory[i];
		printf("%X%X ", (c & 0xF0) >> 4, c & 0x0F);
		j++;
		if (j % 8 == 0) {
			j = 0;
			printf("\n%d  ", i / 8 + 1);
		}
	}*/





	/*for (i = 0; i < filesLoaded; i++) {
		printf("File %s\n", argc[i + 2]);
		rel_entry **reltabela = all_files_data[i]->rel_table;
		int relnum = all_files_data[i]->relocationNumber;

		for (i = 0; i < relnum; i++)
			printf("%d\t%d\t%d\t%d\t%d\n", reltabela[i]->section, reltabela[i]->offset, reltabela[i]->type, reltabela[i]->bytes,
				reltabela[i]->value);
	}*/

	/*printf("\n\n");
	j;
	for (j = 0; j < addedSymbols; j++) {
		printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", linkedSymbolTable[j]->name,
			linkedSymbolTable[linkedSymbolTable[j]->section]->name,
			linkedSymbolTable[j]->offset, linkedSymbolTable[j]->binding,
			linkedSymbolTable[j]->defined, linkedSymbolTable[j]->ord);
	}*/
	/*for (j = 0; j < filesLoaded; j++) {
		printf("*******%s*******\n", argc[2 + j]);
		int i = 0;
		printf("#Tablea simbola\n");
		printf("%16s\t%s\t%s\t%s\t%s\t%s\n", "name", "section", "offset", "bind", "def", "ord");
		sym_table = all_files_data[j]->sym_table;
		section_table = all_files_data[j]->section_table;
		symbolCounter = all_files_data[j]->symbolNumber;
		sectionCounter = all_files_data[j]->sectionNumber;




		for (; i < addedSymbols; i++) {
			printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", sym_table[i]->name,
				sym_table[i]->section != -1 ? sym_table[section_table[sym_table[i]->section]->symbol_number]->name : "UND",
				sym_table[i]->offset, sym_table[i]->binding, sym_table[i]->defined, sym_table[i]->ord);

			printf("%s\n", sym_table[i]->name);
			printf("%d\n", sym_table[i]->section);
			printf("%d\n", sym_table[i]->offset);
			printf("%d\n", sym_table[i]->binding);
			printf("%d\n", sym_table[i]->defined);
			printf("%d\n", sym_table[i]->ord);

		}

		printf("#Tabela sekcija\n");
		for (i = 0; i < sectionCounter; i++) {
			printf("%16s\t%d\t%d\n", sym_name_table[section_table[i]->name]->name, section_table[i]->base, section_table[i]->length);
		}
	}*/


	//	link(argv - 2, argc + 2);

	//system("PAUSE");
	return 0;
}
