/* 
 * This file is part of mq.
 * mq - src/message.c
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
#include <sys/socket.h>
#include <string.h>

#include "message.h"
#include "utils.h"

/* Private methods */
char * _message_get_next_line (Message * self);

/* 
 * Create and initialise a message
 * args:   message type, message string
 * return: Message object or NULL on error
 */
Message * message_new (MessageType type, char * content, int sock)
{
	Message * message = malloc0 (sizeof (Message));

	if (message == NULL)
		return NULL;

	message->_type = type;
	message->_content = content;
	message->sock = sock;

	return message;
}

/*
 * Delete and free a Message
 * args:   Message
 * return: void
 */
void message_del (Message * self)
{
	free (self->_content);
	free (self);
}

/*
 * Send the current message on the socket
 * args:   Message
 * return: 0 on success, 1 on eror
 */
int message_send (Message * self)
{
	char * line; 
	MessageType ptype;

	/* Check that the socket is valid */
	if (self->sock < 1)
		return 1;

	/* Set the type send to prefix content lines */
	if (self->_type == OK)
		ptype = OUT;
	else
		ptype = ERR;

	/* Send each line from the content (if any) */
	while ((line = _message_get_next_line (self)) != NULL)
	{
		/* Send the message type */
		if (send (self->sock, &ptype, sizeof (MessageType), 0) == -1) {
			free (line);
			return 1;
		}

		/* Send the line */
		if (send (self->sock, line, strlen (line), 0) == -1) {
			free (line);
			return 1;
		}

		free (line);
	}

	/* Send the "exit code" */
	if (send (self->sock, &self->_type, sizeof (MessageType), 0) == -1)
		return 1;

	return 0;
}


/* Private methods */

/* 
 * Return the next line from message
 * args:   Message
 * return: next line or NULL
 */
char * _message_get_next_line (Message * self)
{
	static int start = 0;
	char * line, * startp;
	int len;
	
	if (self->_content == NULL)
		return NULL;

	/* Set the starting pointer position */
	startp = self->_content + start;

	/* Count the number of char until the first \n */
	for (len = 0; startp[len] != '\0' && startp[len] != '\n'; len++)
		;

	/* If we're at \0 we've already sent the last line in the previous call */
	if (startp[len] == '\0') {
		/* Reset start index */
		start = 0;
		return NULL;
	}

	line = malloc0 (sizeof (char *) * (len + 2));
	if (line == NULL)
		return NULL;	/* FIXME: the error should be reported somehow */
		
	strncpy (line, startp, len + 2);
	line[len + 1] = '\0';
	
	/* Advance the starting point */
	start += len + 1;

	return line;
}
