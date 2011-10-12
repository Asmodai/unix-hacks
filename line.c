/*
 * line.c --- Read one line from user input.
 *
 * Copyright (c) 2011 Paul Ward <asmodai@gmail.com>
 *
 * Time-stamp: <Tuesday Oct 11, 2011 10:39:44 asmodai>
 * Revision:   49
 *
 * Author:     Paul Ward <asmodai@gmail.com>
 * Maintainer: Paul Ward <asmodai@gmail.com>
 * Created:    09 Apr 2011 06:36:38
 * Keywords:   
 * URL:        not distributed yet
 */
/* {{{ License: */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see  <http://www.gnu.org/licenses/>.
 */
/* }}} */
/* {{{ Commentary: */
/*
 *
 */
/* }}} */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <locale.h>

static int status;                 /* Exit status. */
static int timeout = -1;           /* Timeout value. */

/*
 * Purpose:   Signal handler for timeout and interrupt.
 * Arguments: signum - The signal number.
 * Returns:   Nothing.
 */
static void
handler(int signum)
{
}

/*
 * Purpose:   Read a line in from a given file descriptor.
 * Arguments: fd - The file descriptor to read from.
 * Returns:   Nothing.
 */
static void
doline(int fd)
{
  char ch;
  struct sigaction act;

  /* Clear the memory for the signal action. */
  memset(&act, 0, sizeof(act));

  /* Flush stdout */
  fflush(stdout);

  /*
   * If a timeout is given and is above zero, then we should
   * $initialize the signal handler and start the alarm countdown.
   */
  if (timeout > 0) {
    act.sa_handler = handler;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
      perror("sigaction");
      exit(EXIT_FAILURE);
    }
    alarm(timeout);
  }

  /* Read in the line. */
  for (;;) {
    if (read(fd, &ch, (size_t)1) <= 0) {
      if (errno != EINTR)
        status = 1;
      break;
    }

    /* Handle EOF */
    if (ch == EOF) {
      status = 1;
      break;
    }

    /* Terminates upon newline. */
    if (ch == '\r' || ch == '\n')
      break;

    putchar(ch);
    fflush(stdout);
  }

  /* If we get this far, we have what we need and can kill the alarm. */
  if (timeout > 0)
    alarm(0);

  /* Write out a newline. */
  putchar('\n');
}

/*
 * Main routine.
 */
int
main(int argc, char **argv)
{
  /* If we have 3 arguments, then we have a timeout. */
  if (argc == 3) {
    if (strncmp(argv[1], "-t", 2) == 0) {
      timeout = atoi(argv[2]);
    }
  }

  /* To be nice, set the locale. */
  setlocale(LC_ALL, "");

  /* Read in the line. */
  doline(STDIN_FILENO);

  /* Return the status. */
  return status;
}

/* line.c ends here */
