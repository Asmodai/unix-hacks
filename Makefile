# -*- Mode: Makefile -*-
#
# Makefile --- Unix utilities
#
# Copyright (c) 2011 Paul Ward <asmodai@gmail.com>
#
# Time-stamp: <Saturday Jan  5, 2013 11:02:54 asmodai>
# Revision:   3
#
# Author:     Paul Ward <asmodai@gmail.com>
# Maintainer: Paul Ward <asmodai@gmail.com>
# Created:    11 Oct 2011 10:31:04
# Keywords:   
# URL:        not distributed yet
#
# {{{ License:
#
# This program is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public
# Licenseas published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any
# later version.
#
# This program isdistributed in the hope that it will be
# useful, but WITHOUT ANY  WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.
#
# }}}
# {{{ Commentary:
#
# }}}

CC=gcc
RM=rm
STRIP=/usr/bin/strip

CFLAGS=-Wall -pedantic -O2

ttytype_OBJS=ttytype.o
line_OBJS=line.o
rawline_OBJS=rawline.o
jot_OBJS=jot.o

ttytype_BIN=ttytype
line_BIN=line
rawline_BIN=rawline
jot_BIN=jot

all: ttytype line rawline jot

ttytype: $(ttytype_OBJS)
	$(CC) $(CFLAGS) $(ttytype_OBJS) -o $(ttytype_BIN)

line: $(line_OBJS)
	$(CC) $(CFLAGS) $(line_OBJS) -o $(line_BIN)

rawline: $(rawline_OBJS)
	$(CC) $(CFLAGS) $(rawline_OBJS) -o $(rawline_BIN)

jot: $(jot_OBJS)
	$(CC) $(CFLAGS) $(jot_OBJS) -o $(jot_BIN)

strip: ttytype line rawline
	$(STRIP) $(ttytype_BIN); \
	$(STRIP) $(line_BIN); \
	$(STRIP) $(rawline_BIN); \
	$(STRIP) $(jot_BIN)

clean:
	$(RM) *.o *~ $(ttytype_BIN) $(line_BIN) $(rawline_BIN) $(jot_BIN)

# Makefile ends here

