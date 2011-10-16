/* 
 * This file is part of mq.
 * mq - src/messagelist.h
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

#ifndef MESSAGELIST_H
#define MESSAGELIST_H

#include "list.h"
#include "message.h"

/* MessageList is a wrapper around List which provides new methods */
typedef List MessageList;

/* Wrappers to List's methods */
MessageList * messagelist_new (void);
void messagelist_delete(MessageList * self);
int messagelist_append (MessageList * self, Message * message);
int messagelist_remove (MessageList * self, Message * message);
Message * messagelist_get_message (MessageList * self, int index);

/* New methods */
Message * messagelist_get_message_by_sock (MessageList * self, int sock);

#endif /* MESSAGELIST_H */
