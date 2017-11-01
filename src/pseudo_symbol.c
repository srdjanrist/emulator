#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"

pseudo_symbol *pseudo_head = 0;
pseudo_symbol *pseudo_tail = 0;

int countPS = 0;

void addPseudoSymbol(char * name, int offset ) {
	pseudo_symbol *newelem = (pseudo_symbol*) calloc(1, sizeof(pseudo_symbol));
	symbol_entry* real_sym_entry = (symbol_entry*)calloc(1, sizeof(symbol_entry));
	real_sym_entry->section = 0;
	real_sym_entry->offset = offset;
	real_sym_entry->binding = GLOBAL;
	real_sym_entry->defined = 1;
	real_sym_entry->ord = 0;

	strcpy(real_sym_entry->name, name);
	strcpy(newelem->name, name);
	newelem->real_entry = real_sym_entry;
	newelem->next = 0;
	if (pseudo_head == 0){
		pseudo_head = pseudo_tail = newelem;
	}
	else {
		pseudo_tail->next = newelem;
		pseudo_tail = newelem;
	}
	countPS++;
}

symbol_entry* removePseudoSymbol() {
	pseudo_symbol *ret = pseudo_head;
	pseudo_head = pseudo_head->next;
	if (pseudo_head == 0)
		pseudo_tail = 0;
	countPS--;
	return ret->real_entry;
}

int isEmpty() {
	return pseudo_head ? 0 : 1;
}


int checkPseudoExistance(char * name, int* value) {
	pseudo_symbol *tek = pseudo_head;
	while (tek != 0) {
		if (strcmp(tek->name, name) == 0) {
			*value = tek->real_entry->offset;
			return 1;
		}
		tek = tek->next;
	}
	return 0;
}
