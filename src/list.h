/* 
 * This file is part of mq.
 * mq - src/list.h
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

#ifndef LIST_H
#define LIST_H

#define CHUNK_SIZE	10	/* amount to increase/decrease list's size */


typedef struct _List List;

struct _List 
{
	void ** _list;
	int _len;
};

List * list_new (void);
int list_len (List * self);
int list_append (List * self, void * item);
int list_remove (List * self, void * item);
void * list_get_item (List * self, int index);

#endif /* LIST_H */
