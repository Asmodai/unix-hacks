/*
 * ttytype.c --- Terminal type identification utility
 *
 * Copyright (c) 2011 Paul Ward <asmodai@gmail.com>
 *
 * Time-stamp: <Thursday Oct 27, 2011 15:17:27 asmodai>
 * Revision:   187
 *
 * Author:     Paul Ward <asmodai@gmail.com>
 * Maintainer: Paul Ward <asmodai@gmail.com>
 * Created:    10 Oct 2011 04:00:12
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
#include <ctype.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <stdarg.h>

#include <sys/param.h>
#include <sys/ioctl.h>

/* ================================================================== */
/* {{{ Definitions: */

#ifndef FALSE
# define FALSE        0
#endif

#ifndef TRUE
# define TRUE         1
#endif

#define TTY_RESET     0
#define TTY_RAW       1

#define TERM_UNKNOWN  0
#define TERM_ANSI     1
#define TERM_HP       2
#define TERM_WYSE     3

/* }}} */
/* ================================================================== */

/* ================================================================== */
/* {{{ Global variables: */

/*
 * Terminal path and file handle for use with the various output
 * routines.
 */
static char ttyfile[MAXPATHLEN] = { "/dev/tty\0" };
static FILE *ttystdout;                 /* Points to /dev/tty. */

/* Argument flags */
static int aflag = FALSE;               /* -a was passed. */
static int pflag = FALSE;               /* -p was passed. */
static int sflag = FALSE;               /* -s was passed. */
static int vflag = FALSE;               /* -v was passed. */
static int tflag = FALSE;               /* -t was passed. */
static int dflag = FALSE;               /* -d was passed. */

/* Table for pretty-printing escape codes. */
static char escapes[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_\0";

/*
 * NB: this number is totally arbitrary.  It was obtained by
 * experimenting with `ttytype' on HP-UX.
 */
static char term[40] = { 0 };

/*
 * Terminal state.
 */
static int ttyState = TTY_RESET;        /* Terminal state. */

/*
 * Structure to store terminal states while we tickle things with
 * tcsetattr et al.
 */
static struct termios termAttribs, termAttribsSaved;

/*
 * Storage of side-effects.  These are set when/if we find what we're
 * looking for.
 */
static int gotTerm = FALSE;
static int termType = TERM_UNKNOWN;

/* The lines/columns this terminal currently has. */
static int defaultLines = 24;           /* Default No. of lines. */
static int defaultColumns = 80;         /* Default No. of columns. */
static int lines = -1;                  /* Number of lines. */
static int columns = -1;                /* Number of columns. */

/* Saved program name. */
static char *progname = NULL;

/* }}} */
/* ================================================================== */

/* ================================================================== */
/* {{{ Terminal information: */

typedef struct {
  char bytes[8];                        /* DECID/DECDA */
  char *terminal;                       /* Terminal string. */
} termTypes;

/* ------------------------------------------------------------------ */
/* {{{ ANSI terminals: */

termTypes ANSI[] = {
  /*
   * DEC VT5x family.
   *
   * Note that at least one VT52 terminal emulator does not return a
   * DECID.
   *
   * VT50    - ^[/A
   * VT55    - ^[/C
   * VT50H   - ^[/H
   * VT50H   - ^[/J (same as above, but with enhanced capabilities)
   * VT52    - ^[/K
   * VT52    - ^[/L (same as above, but with enhanced capabilities)
   */
  { { 0x1b, 0x2f, 0x00 },       "vt52\0"  },
  { { 0x1b, 0x2f, 0x41, 0x00 }, "vt50\0"  },
  { { 0x1b, 0x2f, 0x43, 0x00 }, "vt55\0"  },
  { { 0x1b, 0x2f, 0x48, 0x00 }, "vt50h\0" },
  { { 0x1b, 0x2f, 0x4a, 0x00 }, "vt50h\0" },
  { { 0x1b, 0x2f, 0x4b, 0x00 }, "vt52\0"  },
  { { 0x1b, 0x2f, 0x4c, 0x00 }, "vt52\0"  },

  /*
   * DEC VT100+
   *
   * The tests this code makes at this present time does not delve
   * deeper into terminal families beyond the base class (e.g. if you
   * have a VT525, ttytype will report it as a VT510.  This is mostly
   * because there is no reliable way of determing the exact type of
   * terminal from the device attributes string alone.  The only way
   * of achieving this would be to have a sure-fire list of device
   * attributes for each device, which DEC never bothered providing -
   * terminals in the same family can have very similar DA strings,
   * making differentiating very difficult, believe me... I have
   * tried.
   *
   * VT100   - ^[[?1;0c  (also a VT101, but we'd rather report VT100)
   *           ^[[?1;1c
   *           ^[[?1;2c
   *           ^[[?1;3c
   *           ^[[?1;4c
   *           ^[[?1;5c
   *           ^[[?1;6c
   *           ^[[?1;7c
   * VT102   - ^[[?6c
   * VT220   - ^[[?62;
   * VT320   - ^[[?63;
   * VT420   - ^[[?64;
   * VT510   - ^[[?65;
   */
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x30, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x31, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x32, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x33, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x34, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x35, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x36, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x31, 0x3b, 0x37, 0x63, 0x00 }, "vt100\0" },
  { { 0x1b, 0x5b, 0x3f, 0x36, 0x63, 0x00 },             "vt102\0" },
  { { 0x1b, 0x5b, 0x3f, 0x36, 0x32, 0x3b, 0x00 },       "vt220\0" },
  { { 0x1b, 0x5b, 0x3f, 0x36, 0x33, 0x3b, 0x00 },       "vt320\0" },
  { { 0x1b, 0x5b, 0x3f, 0x36, 0x34, 0x3b, 0x00 },       "vt420\0" },
  { { 0x1b, 0x5b, 0x3f, 0x36, 0x35, 0x3b, 0x00 },       "vt510\0" },
  
  { { 0x00 }, 0x00 }
};

/* }}} */
/* ------------------------------------------------------------------ */

/*
 * NOTE:  Wyse and HP terminal tables ought to exist here, but I do
 * not actually own any terminal hardare from either manufacturer to
 * test, therefore I am resorting to some simple asumptions.  See the
 * HP and Wyse detection functions for more information.
 */

/* }}} */
/* ================================================================== */

/* ================================================================== */
/* {{{ Utility functions: */

/*
 * Purpose:   Wrapper around `perror'.
 * Arguments: fmt  - Standard format string.
 *            args - Standard format args.
 * Returns:   Nothing.
 */
static void
perrorf(const char *fmt, ...)
{
  va_list ap;
  char *ptr;

  va_start(ap, fmt);
  vasprintf(&ptr, fmt, ap);
  perror(ptr);
  free(ptr);
  va_end(ap);
}

/*
 * Purpose:   A wrapper around malloc(3) that checks for memory
 *            exhaustion.
 * Arguments: n - The size of the memory region to allocate.
 * Returns:  The newly-allocated object.
 */
static void *
xmalloc(size_t n)
{
  void *p;
  
  p = malloc(n);
  if (p == 0) {
    perrorf("Memory exhausted while allocating %u bytes.", n);
    exit(EXIT_FAILURE);
  }

  return p;
}

/*
 * Purpose:   Convert an escape sequence to printable characters.
 * Arguments: dest - The destinations tring.
 *            src  - The source string.
 *            size - The size of the destination string.
 * Returns:   A pointer to the destination string.
 */
static char *
prettyPrint(char *dest, const char *src, size_t size)
{
  size_t idx, cnt;

  idx = cnt = 0;

  /* Zero out the destination buffer. */
  memset(dest, '\0', size);

  /* Perform the pretty-printing. */
  for (idx = 0; src[idx] != '\0'; idx++) {
    /* Exit if we've passed the upper bound. */
    if (cnt >= size)
      break;

    /* The pretty-print process. */
    if (src[idx] <= 0x1f) {
      /*
       * If the character at the index is an escape sequence, then
       * convert it into something printable.
       */
      int off = src[idx];

      /* Prefix escapes with ^ */
      *dest++ = '^';
      *dest++ = escapes[off];

      /* Increment the count by the number of characters written. */
      cnt += 2;
    } else if (src[idx] == 0x7f) {
      /* 0x7F needs pretty-printing too. */
      *dest++ = '^';
      *dest++ = '?';
      cnt += 2;
    } else {
      /* We're not an escape code, so just add it as-is. */
      *dest++ = src[idx];
      cnt++;
    }
  }

  /* Handle null-termination. */
  if (cnt > size)
    dest[size] = '\0';
  else
    dest[cnt] = '\0';

  /* Finally return the pretty-printed string. */
  return dest;
}

/*
 * Purpose:   Set the terminal to raw mode.
 * Arguments: fd - The file descriptor to use.
 * Returns:   TRUE if the operation is sucessful; otherwise FALSE.
 */
static int
setRaw(int fd)
{
  /* Attempt to get the current terminal attributes. */
  if (tcgetattr(fd, &termAttribsSaved) < 0) {
    perror("tcgetattr");
    return FALSE;
  }

  /* Set the required terminal flags for non-canonical (raw). */
  bzero(&termAttribs, sizeof(termAttribs));

  /*
   * INPUT MODES:
   *
   *     BRKINT    - No breaks.
   *     ICRNL     - No CR->NL.
   *     INPCK     - No parity check.
   *     ISTRIP    - No strip character.
   *     IXON      - No flow control.
   */
  termAttribs.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  /*
   * OUTPUT MODES:
   *
   *     OPOST     - No post-processing.
   */
  termAttribs.c_oflag &= ~(OPOST);

  /*
   * CONTROL MODES:
   *
   *     CS8       - Set 8-bit characters.
   */
  termAttribs.c_cflag &= ~(CSIZE | PARENB);
  termAttribs.c_cflag |= (CS8);

  /*
   * LOCAL MODES:
   *
   *     ECHO      - No echo.
   *     ICANON    - Canonical mode off.
   *     IEXTEN    - No extended functions.
   *     ISIG      - No signal characters.
   */
  termAttribs.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /*
   * Let's set our return condition.  No timer, 1 byte.
   */
  termAttribs.c_cc[VTIME] = 0;
  termAttribs.c_cc[VMIN] = 1;

  /* Flush the terminal. */
  tcflush(fd, TCIFLUSH);

  /* Set the attributes. */
  if (tcsetattr(fd, TCSANOW, &termAttribs) < 0) {
    perror("tcsetattr");
    return FALSE;
  }

  /* We're in raw mode now. */
  ttyState = TTY_RAW;

  /* We're done. */
  return TRUE;
}

/*
 * Purpose:   Reset the terminal to the previous mode.
 * Arguments: fd - The file descriptor to use.
 * Returns:   TRUE if the operation was successful; otherwise FALSE.
 */
static int
setPrevious(int fd)
{
  /* Ignore this if we've not been set to raw mode with `setRaw'. */
  if (ttyState != TTY_RAW) 
    return TRUE;

  /*
   * Ok, now, we simply restore the terminal attributes to those saved
   * when we did `setRaw', then flush it.
   */
  if (tcsetattr(fd, TCSAFLUSH, &termAttribsSaved) < 0) {
    perror("tcsetattr");
    return FALSE;
  }

  /* Flush the terminal. */
  tcflush(fd, TCIFLUSH);

  /* Let things know we've been reset. */
  ttyState = TTY_RESET;

  /* And we're done. */
  return TRUE;
}

/*
 * Purpose:   Read from stdin in a normal manner, but ignoring
 *            anything non-alphanumeric.
 * Arguments: buf  - The input buffer.
 *            size - The maximum no. of characters to read.
 * Returns:   The number of characters read.
 */
static size_t
readterm(char *buf, size_t size)
{
  ssize_t rtn = 0;
  char ch = 0;

  /* Simply loop over input, until we receive a CR */
  while (1) {
    ch = fgetc(stdin);

    if (isalnum(ch) && ch != '\n') {
      /* We're only interested in alphanumerics */
      if (rtn < size) {
        /* We still have space in the buffer, so continue. */
        *buf++ = ch;
        rtn++;
      } else {
        /* No more space, so we're out. */
        return size;
      }
    } else if (ch == '\n') {
      /* We received a newline */
      return rtn;
    }
  }

  /* We shouldn't get here. */
  return rtn;
}

/*
 * Purpose:   Prints out a message to a terminal and then waits for input.
 * Arguments: buf   - The input buffer.
 *            size  - The size of the input buffer.
 *            fmt   - A varargs format buffer.
 *            ...   - A varargs argument list.
 * Returns:   The size of the read buffer.
 */
static size_t
rawread(char *buf, size_t size, const char *fmt, ...)
{
  ssize_t rtn = -1;
  int idx = 0;
  int res = -1;
  struct timeval tout;
  fd_set fs;
  va_list ap;
  char *tmp;

  va_start(ap, fmt);

  /* Print the format string to a temporary buffer. */
  vasprintf(&tmp, fmt, ap);

  /* Set up the timeout. */
  tout.tv_sec = 1;
  tout.tv_usec = 0;

  /* Set up the file descriptors for select(). */
  FD_ZERO(&fs);
  FD_SET(STDIN_FILENO, &fs);

  /* Set terminal to raw mode. */
  setRaw(STDIN_FILENO);

  /* Print to terminal and flush Flush buffers */
  fprintf(ttystdout, tmp);
  fflush(ttystdout);

  /* Loop here while waiting for input. */
  while (res != 0 || res == EINTR) {  
    res = select(STDIN_FILENO + 1, &fs, NULL, NULL, &tout);
    if (FD_ISSET(STDIN_FILENO, &fs) != 0) {
      rtn = read(STDIN_FILENO, buf + idx, (size_t)1);

      if (rtn > 0)
        idx += rtn;
        
      if (idx >= size) {
        idx = size;
        break;
      }
    }
  }

  /* Restore the terminal. */
  setPrevious(STDIN_FILENO);

  /* If we have -D, we get to pretty-print things */
  if (dflag) {
    char *pretty;

    if (idx > 0) {
      pretty = xmalloc(sizeof(char) * (idx * 2));
      prettyPrint(pretty, buf, (idx * 2));
    }

    fprintf(stderr,
            "%s: read %d characters: \"%s\"\n",
            progname,
            (idx > 0) ? idx : 0,
            (idx > 0) ? pretty : "");

    if (idx > 0)
      free(pretty);

    fflush(stderr);
  }
  
  /* We're done. */
  return (idx > 0) ? idx : 0;
}

/* }}} */
/* ================================================================== */

/* ================================================================== */
/* {{{ Terminal detection: */

/* ------------------------------------------------------------------ */
/* {{{ Wyse routines: */

/*
 * Purpose:   Attempt to identify a Wyse terminal.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void 
identWyse(void)
{
  char ctlseq[] = { 0x1b, ' ', 0x00 };   /* Terminal ID */
  char result[128] = { 0 };
  int size = -1;

  /* Flush the tty. */
  fflush(ttystdout);

  /* Write the control sequence. */
  if ((size = rawread(result, 128, "%s", ctlseq)) == 0) {
    /* No luck. */
    gotTerm = FALSE;
  } else {
    snprintf(term, 128, "wy%s", result);
    gotTerm = TRUE;
    termType = TERM_WYSE;
  }

  /* IF we're verbose, then display what we found. */
  if (gotTerm && vflag) {
    fprintf(stderr, 
            "%s: WYSE terminal response \"%s\"\n",
            progname,
            term);
    fflush(stderr);
  }
}

/*
 * Purpose:   Attempt to find the number of lines and columns.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
sizeWyse(void)
{
  /*
   * I have no idea how to do this... anyone have a Wyse manual that
   * details all the control sequences?
   */
}

/* }}} */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* {{{ ANSI routines: */

/*
 * Purpose:   Attempt to identify an ANSI/DEC terminal.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
identANSI(void)
{
  char DECID[] = { 0x1b, 'Z', 0x00 };
  char DECDA[] = { 0x1b, '[', '0', 'c', 0x00 };
  char result[128] = { 0 };
  int size = -1;

  /*
   * This is a bit crazy-go-nuts.  For VT100s and below, we can use
   * DECID to ascertain the terminal ID, but this is not
   * forward-compatible with newer terminals.  For those we have to
   * obtain the device attributes.
   *
   * There is a possiblity of some devices understanding both DECID
   * and DA, so we want to send DA first.  If we get a response from
   * DA we can deal with that, resorting to DECID only if we're still
   * at a loss as to what terminal we are.  This way we avoid multiple
   * responses.
   */

  /* Ok, so check for the primary device attributes response. */
  if ((size = rawread(result, 128, "%s", DECDA)) == 0) {
    /* No device attributes... so try DECID. */
    if ((size = rawread(result, 128, "%s", DECID)) == 0) {
      /* No DECID either. */
      gotTerm = FALSE;
      return;
    }
  }

  /*
   * Now we should have a resonse of some sort to check against the
   * table of DEC terminal IDs.
   */
  {
    int idx = 0;

    for (idx = 0; idx < (sizeof(ANSI) / sizeof(ANSI[0])); idx++) {
      if (strncmp(result, ANSI[idx].bytes, strlen(ANSI[idx].bytes)) == 0) {
        /* Bingo. */
        strncpy(term, ANSI[idx].terminal, strlen(ANSI[idx].terminal));
        termType = TERM_ANSI;
        gotTerm = TRUE;
        break;
      }
    }

    /* If we're verbose, report what we have. */
    if (gotTerm & vflag) {
      char pretty[128] = { 0 };

      prettyPrint(pretty, result, 128);
      fprintf(stderr,
              "%s: ANSI terminal response \"%s\" mapped to \"%s\"\n",
              progname,
              pretty,
              term);
      fflush(stderr);
    }
  }
}

/*
 * Purpose:   Attempt to find the number of rows/columns.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
sizeANSI(void)
{
  /*
   * This series of control sequences attempts to compute the size of
   * the terminal display area.  The commands are:
   *
   * 1)   Save the current cursor position (DECSC)
   * 2)   Move the cursor to 999,999 (CUP)
   * 3)   Ascertain the current cursor position (CPR)
   * 4)   Restore the saved cursor position (DECRC)
   *
   * This should result in the maximum number of addressable lines and
   * columns being returned by the terminal.
   */
  char ctlseq[] = {
    0x1b, '7',                                              /* DECSC */
    0x1b, '[', '9', '9', '9', ';', '9', '9', '9', 'H',      /* CUP */
    0x1b, '[', '6', 'n',                                    /* CPR */
    0x1b, '8', 0x00                                         /* DECRC */
  };
  char result[128] = { 0 };
  int size = -1;

  /* Flush the tty. */
  fflush(ttystdout);

  /* Write the control sequence. */
  if ((size = rawread(result, 128, "%s", ctlseq)) > -1) {
    /* TODO... parse the result. */
  }
}

/* }}} */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* {{{ HP routines: */

/*
 * Purpose:   Attempt to identify a HP termninal.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
identHP(void)
{
  char ctlseq[] = { 0x1b, '*', 's', '1', '^', 0 };
  char result[128] = { 0 };
  int size = -1;

  /* Flush the tty. */
  fflush(ttystdout);

  /* Attempt to read a reply. */
  if ((size = rawread(result, 128, "%s", ctlseq)) == 0) {
    /* Terminal probably isn't a HP. */
    gotTerm = FALSE;
  } else {
    /*
     * HP terminals usually return the number or name of the
     * terminal.  I do not have any real HP terminals that I can test
     * for responses, so this is using Blind Guesses(tm).
     */

    if (!isalnum(term[size])) {
      size--;
    }

    strncpy(term, result, size);
    gotTerm = TRUE;
    termType = TERM_HP;
  }
  
  /* If we're verbose (-v), print out what we have. */
  if (gotTerm && vflag) {
    fprintf(stderr,
            "%s: HP terminal response \"%s\"\n",
            progname,
            term);
    fflush(stderr);
  }  
}

/*
 * Purpose:   Attempt to find the number of rows and columns.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
sizeHP(void)
{
  /*
   * This is probably a kludge.  The control sequence crafted here
   * simply attempts to move the cursor to some ludicrous screen
   * position.  This will never happen, instead the cursor will
   * deposit itself at the maximum possible extents.
   */
  char ctlseq[] = {
    0x1b, '&', 'a', '9', '9', '9', 'c', '9', '9', '9', 'Y', 0
  };
  /*
   * However, that is only part of the solution.  The following
   * sequence will query the terminal for the current cursor position.
   */
  char query[] = { 0x1b, '`', 0 };
  char result[128] = { 0 };
  int size = 0;

  /* Flush the tty. */
  fflush(ttystdout);

  /* Write out the query and read any result. */
  if ((size = rawread(result, 128, "%s", query)) > -1) {
    /*
     * Ok, so this worked... now let's try to set the cursor
     * position.
     */
    if ((size = rawread(result, 128, "%s%s", ctlseq, query)) > -1) {
      /*
       * The result is in the form of ^[&aCCCcLLLY where CCC is the
       * number of columns and LLL is the number of lines.
       */
      if (result[0] == 0x1b &&
          result[1] == '&' &&
          result[2] == 'a')
      {
        char *tok, *beg = (result + 3);

        tok = strtok(beg, "c");
        columns = atoi(tok) + 1;

        tok = strtok(NULL, "c");
        lines = atoi(tok) + 1;

        /* If we're debugging, print a little info. */
        if (dflag) {
          fprintf(stderr,
                  "%s: HP reports size as %d lines, %d columns\n",
                  progname,
                  lines,
                  columns);
          fflush(stderr);
        }
      }
    }
  }
}

/* }}} */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* {{{ ioctl routines: */

/*
 * Purpose:   Attempt to obtain the terminal size using ioctl.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
sizeIoctl(void)
{
  struct winsize w;

  bzero(&w, sizeof(w));
  ioctl(0, TIOCGWINSZ, &w);

  lines = w.ws_row;
  columns = w.ws_col;

  if (dflag) {
    fprintf(stderr,
            "%s: ioctl reports %d lines, %d columns.\n",
            progname,
            lines,
            columns);
    fflush(stderr);
  }
}

/* }}} */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* {{{ Screen size detection: */

/*
 * Purpose:   Attempt to ascertain the size of the terminal.
 * Arguments: None.
 * Returns:   Nothing.
 */
static void
screensize(void)
{

  /*
   * If we have a terminal, then try the size computation routine for
   * the relevant terminal type.
   */
  if (gotTerm) {
    if (termType == TERM_WYSE)
      sizeWyse();
    else if (termType == TERM_ANSI)
      sizeANSI();
    else if (termType == TERM_HP)
      sizeHP();
  }

  /*
   * If lines or columns is equal to -1 then something failed so try
   * getting the size via ioctl.
   */
  if (lines == -1 || columns == -1)
    sizeIoctl();

  /*
   * If lines or columns is still equal to -1 then something really
   * failed so just use the default values.
   */
  if (lines == -1 || columns == -1) {
    lines = defaultLines;
    columns = defaultColumns;
  }

  /* If we're verbose, display some info. */
  if (vflag) {
    fprintf(stderr,
            "%s: COLUMNS=%d; LINES=%d\n",
            progname,
            columns,
            lines);
    fflush(stderr);
  }
}

/* }}} */
/* ------------------------------------------------------------------ */



/* }}} */
/* ================================================================== */

/* ================================================================== */
/* {{{ Main routine: */

int
main(int argc, char **argv)
{
  int c;
  int restrictANSI = FALSE;     /* True if we're only checking for ANSI */
  int restrictHP = FALSE;       /* True if we're only checking for HP */
  int restrictWyse = FALSE;     /* True if we're only checking for Wyse */

  /* Save the program name for later. */
  progname = (char *)basename((const char *)argv[0]);

  /* Clear opterr. */
  opterr = 0;

/* .................................................................. */
/* {{{ Parse optargs: */

  /* Parse optargs. */
  while ((c = getopt(argc, argv, "aDpsvt:T:C:R:")) != -1) {
    switch (c) {
      /*
       * Start with the documented options.
       */
      case 'a':   /* Use `unknown' as TERM rather than prompting. */
        aflag = TRUE;
        break;
      case 'p':   /* Prompt for terminal type rather than detecting. */
        pflag = TRUE;
        break;
      case 's':   /* Issue commands for a shell to use. */
        sflag = TRUE;
        break;
      case 'v':   /* Verbose mode. */
        vflag = TRUE;
        break;
      case 't':   /* Restrict enquiry to a specific terminal type. */
        tflag = TRUE;

        if (strncmp(optarg, "hp", 2) == 0)
          restrictHP = TRUE;
        else if (strncmp(optarg, "ansi", 4) == 0)
          restrictANSI = TRUE;
        else if (strncmp(optarg, "wyse", 4) == 0)
          restrictWyse = 1;

        break;

      /*
       * Now for the undocumented options.
       */
      case 'D':   /* Debug mode. */
        dflag = TRUE;
        break;
      case 'T':   /* TTY device file to use. */
        strncpy(ttyfile, optarg, MAXPATHLEN);
        break;
      case 'R':   /* Default number of rows. */
        defaultLines = atoi(optarg);
        break;
      case 'C':   /* Default number of columns. */
        defaultColumns = atoi(optarg);
        break;
   
      /*
       * Default case.
       */
      default:
        fprintf(stderr,
                "Usage: %s [-apsv] [-t type]\n",
                progname);
        fflush(stderr);
        exit(2);
    } /* switch (c) */
  } /* while ((c = getopt(... */

/* }}} */
/* .................................................................. */

  /*
   * We want to use the TTY device file for our output so that stdout
   * can be used for other things.
   */
  if ((ttystdout = fopen(ttyfile, "w+")) == NULL) {
    perrorf("%s: Could not open %s", progname, ttyfile);
    
    /* Try using /dev/tty. */
    if ((ttystdout = fopen("/dev/tty", "w+")) == NULL) {
      perrorf("%s: Could not open /dev/tty", progname);
      exit(EXIT_FAILURE);
    }
  }

  /* Make sure that `ttystdout' is a TTY. */
  if (!isatty(fileno(ttystdout))) {
    perrorf("%s: Is not a TTY", progname);
    exit(EXIT_FAILURE);
  }

  memset(term, '\0', 40);

/* .................................................................. */
/* {{{ Handle -p: */

  /*
   * When -p is given we ought to prompt for a terminal type and only
   * resort to using detection should no input be givenm.
   */
  if (pflag) {
    int size = -1;

    fprintf(ttystdout, "TERM = ");
    fflush(ttystdout);
    size = readterm(term, 40);

    if (size > 0)
      gotTerm = TRUE;
  }

/* }}} */
/* .................................................................. */

/* .................................................................. */
/* {{{ Terminal checks: */

  /* Check for Wyse terminals first. */
  if (!gotTerm && (!tflag || (tflag && restrictWyse)))
    identWyse();

  /* Check for ANSI next. */
  if (!gotTerm && (!tflag || (tflag && restrictANSI)))
    identANSI();

  /* Lastly check for HP. */
  if (!gotTerm && (!tflag || (tflag && restrictHP)))
    identHP();

  /* If we still don't have a terminal... */
  if (!gotTerm) {
    /* ... and we're not passed -a... */
    if (!aflag) {
      /* ... then we get to prompt. */
      int size = -1;

      /* Ask the user for the terminal. */
      fprintf(ttystdout, "TERM = (vt100) ");
      fflush(ttystdout);

      /* Get the users' input. */
      size = readterm(term, 40);

      /* Did the user type anything? */
      if (size > 0)
        gotTerm = TRUE;
      else
        strncpy(term, "vt100\0", 6);

      /*
       * If we have verbose, only print out the terminal if the user
       * actually responded to us.
       */
      if (gotTerm && vflag) {
        fprintf(stderr,
                "%s: manual terminal response is \"%s\"\n",
                progname,
                term);
        fflush(stderr);
      }
    } else {
      /* ... settle with `unknown' */
      strncpy(term, "unknown\0", 8);
    }
  }

/* }}} */
/* .................................................................. */

/* .................................................................. */
/* {{{ Handle -s: */

/*
 * When -s is given, we dump out stuff to stdout that can be
 * redirected and used by a script or whatnot.
 */
  if (sflag) {
    char *shell;

    /*
     * Allocate space for the shell then attempt to get it from the
     * environment.
     */
    shell = xmalloc(sizeof(char) * MAXPATHLEN);
    shell = basename((char *)getenv("SHELL"));

    /* Toddle off and get the screen dimensions. */
    screensize();

    /*
     * Use the `SHELL' environment variable to compute exactly how to
     * display the values.
     */
    if (strncmp(shell, "csh", 3) == 0 ||
        strncmp(shell, "tcsh", 4) == 0)
    {
      /* C-Shell and TWENEX C-Shell. */
      fprintf(stdout, "setenv TERM %s\n", term);
      fprintf(stdout, "setenv LINES %d\n", lines);
      fprintf(stdout, "setenv COLUMNS %d\n", columns);
      fflush(stdout);
    } else {
      /* Bourne derivatives and everything else. */
      fprintf(stdout, "TERM=\'%s\'; export TERM;\n", term);
      fprintf(stdout, "LINES=%d; export LINES;\n", lines);
      fprintf(stdout, "COLUMNS=%d; export COLUMNS;\n", columns);
      fflush(stdout);
    }
  } else {
    /* No -s, just print the terminal type. */
    fprintf(stdout, "%s\n", term);
    fflush(stdout);
  }

/* }}} */
/* .................................................................. */

  /* And we're done. */
  return EXIT_SUCCESS;
}

/* }}} */
/* ================================================================== */

/* ttytype.c ends here */

