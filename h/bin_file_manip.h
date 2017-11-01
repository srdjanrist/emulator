#ifndef _BIN_FILE_MANIP_H
#define _BIN_FILE_MANIP_H

enum {
	BIN_EOF, SYMBOL_TABLE, RELOCATION_TABLE, CODE, NEW_SECTION
};

extern int bin_file_sections[];


#endif