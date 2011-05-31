/* 
 * This file is part of mq.
 * mq - src/main.c
 * Copyright (C) 2011 Mathias Andre <mathias@acronycal.org>
 *
 * mq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mq.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "daemon.h"
#include "client.h"

Daemon * d = NULL;

int main (void)
{
	Client * client = client_new ();

	if (client == NULL) {
		perror ("main:client_new");
		return EXIT_FAILURE;
	}

	client_run (client);

	return EXIT_SUCCESS;
}
