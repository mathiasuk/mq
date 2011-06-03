/* 
 * This file is part of mq.
 * mq - src/message.h
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

#ifndef MESSAGE_H
#define MESSAGE_H

#define PID_FILENAME ".mq.pid"
#define SOCK_FILENAME ".mq.sock"
#define LOG_FILENAME ".mq.log"

/* Type of messages sent to client */
typedef enum {
	OK,		/* exit successfully */
	KO,		/* exit unsuccessfully */
	OUT,	/* print message to stdout */
	ERR		/* print message to stderr */
} MessageType;

typedef struct _Message Message;

struct _Message 
{
	MessageType _type;
	char * _content;	/* content of the message */
	int sock;			/* socket the message should be written to */
};

Message * message_new (MessageType type, char * message, int sock);

#endif /* CLIENT_H */
