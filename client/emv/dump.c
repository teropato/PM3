/*
 * libopenemv - a library to work with EMV family of smart cards
 * Copyright (C) 2015 Dmitry Eremin-Solenikov
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "dump.h"

void dump_buffer_simple(const unsigned char *ptr, size_t len, FILE *f) {
	int i;

	if (!f)
		f = stdout;

	for (i = 0; i < len; i ++)
		fprintf(f, "%s%02hhX", i ? " " : "", ptr[i]);
}

void dump_buffer_tab(const unsigned char *ptr, size_t len, FILE *f, int tabs) {
	int i, j;
	char buf[64] = {0};
	memset(buf, ' ', tabs > 64 ? 64 : tabs);

	if (!f)
		f = stdout;

	for (i = 0; i < len; i += 16) {
		fprintf(f, "%s%02x:", buf, i);
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				fprintf(f, " %02hhx", ptr[i + j]);
			else
				fprintf(f, "   ");
		}
		fprintf(f, " |");
		for (j = 0; j < 16 && i + j < len; j++) {
			fprintf(f, "%c", (ptr[i+j] >= 0x20 && ptr[i+j] < 0x7f) ? ptr[i+j] : '.' );
		}
		fprintf(f, "\n");
	}
}

void dump_buffer(const unsigned char *ptr, size_t len, FILE *f) {
	dump_buffer_tab(ptr, len, f, 4);
}

