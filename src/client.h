/* 
 * This file is part of mq.
 * mq - src/client.h
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

#ifndef CLIENT_H
#define CLIENT_H

#define PID_FILENAME ".mq.pid"
#define SOCK_FILENAME ".mq.sock"
#define LOG_FILENAME ".mq.log"

typedef struct _Client Client;

struct _Client 
{
	char * _pid_path;
	char * _sock_path;
	char * _log_path;
	int _argc;
	char ** _argv;
};

Client * client_new (void);
int client_parse_args (Client * self, int argc, char * argv[]);
void client_run (Client * self);

#endif /* CLIENT_H */
