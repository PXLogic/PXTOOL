/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <string.h>

#include "scpi.h"

SR_PRIV const char *sr_scpi_unquote_string(char *s)
{
	size_t s_len;
	char quotes[3];
	char *rdptr;

	if (!s || !*s)
		return s;
	s_len = strlen(s);
	if (s_len < 2)
		return s;

	if (s[0] != '\'' && s[0] != '"')
		return s;
	if (s[0] != s[s_len - 1])
		return s;

	quotes[0] = quotes[1] = *s;
	quotes[2] = '\0';
	s[s_len - 1] = '\0';
	s++;
	rdptr = s;
	while ((rdptr = strstr(rdptr, quotes)) != NULL) {
		memmove(rdptr, rdptr + 1, strlen(rdptr));
		rdptr++;
	}

	return s;
}
