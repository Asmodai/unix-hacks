/*
 * rawline.c --- Read one line from user input in raw mode.
 *
 * Copyright (c) 2011 Paul Ward <asmodai@gmail.com>
 *
 * Time-stamp: <Tuesday Oct 11, 2011 10:52:51 asmodai>
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

#define TTY_RESET        0
#define TTY_RAW          1

static int status;                 /* Exit status. */
static int timeout = -1;           /* Timeout value. */
static struct termios termAttribs, termAttribsSaved;
static int ttyState = TTY_RESET;

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
 * Purpose:   Set the terminal to raw mode.
 * Arguments: fd - the file descriptor of the terminal.
 * Returns:   -1 if tcgetattr/tcsetattr fail,
 *            0 on success.
 */
static int
setRaw(int fd)
{
  int i;

  if ((i = tcgetattr(fd, &termAttribs)) < 0) {
    perror("tcgetattr");
    return -1;
  }

  termAttribsSaved = termAttribs;

  termAttribs.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  termAttribs.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  termAttribs.c_cflag &= ~(CSIZE | PARENB);
  termAttribs.c_cflag |= CS8;
  termAttribs.c_oflag &= ~(OPOST);

  termAttribs.c_cc[VMIN] = 1;
  termAttribs.c_cc[VTIME] = 0;

  if ((i = tcsetattr(fd, TCSANOW, &termAttribs)) < 0) {
    perror("tcsetattr");
    return -1;
  }

  ttyState = TTY_RAW;

  return 0;
}

/*
 * Purpose:   Reset the terminal to cooked mode.
 * Arguments: fd - the file descriptor of the terminal.
 * Returns:   -1 if tcsetattr fails,
 *            0 on success.
 */
static int
setCooked(int fd)
{
  int i;

  if (ttyState != TTY_RAW)
    return 0;

  if ((i = (tcsetattr(fd, TCSAFLUSH, &termAttribsSaved))) < 0) {
    perror("tcsetattr");
    return -1;
  }

  ttyState = TTY_RESET;
  return 0;
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

  /* Set the terminal to raw mode. */
  setRaw(fd);

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

  /* Restore the terminal. */
  setCooked(fd);

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

/* rawline.c ends here */
