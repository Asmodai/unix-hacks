/*
 * jot.c --- Portable implementation of the `jot' utility from BSD.
 *
 * Copyright (c) 2013 Paul Ward <asmodai@gmail.com>
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Time-stamp: <Saturday Jan  5, 2013 11:00:49 asmodai>
 * Revision:   2
 *
 * Author:     Paul Ward <asmodai@gmail.com>
 * Maintainer: Paul Ward <asmodai@gmail.com>
 * Created:    05 Jan 2013 09:36:18
 * Keywords:   
 * URL:        not distributed yet
 */
/* {{{ License: */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer. 
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* }}} */
/* {{{ Commentary: */
/*
 *
 */
/* }}} */

/* Include some BSD-specific stuff. */
#if defined(__bsdi__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
# include <sys/param.h>
# include <sys/cdefs.h>
#endif

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPS_DEF       100
#define BEGIN_DEF      1
#define ENDER_DEF      100
#define STEP_DEF       1

#define is_default(s)  (strcmp((s), "-") == 0)

double      begin;
double      ender;
double      s;
long        reps;
int         randomize;
int         infinity;
int         boring;
int         prec;
int         longdata;
int         intdata;
int         chardata;
int         nosign;
int         nofinalnl;
const char *sepstring = "\n";
char        format[BUFSIZ];

#if !defined(BSD)
# include <sys/types.h>

/**
 * @brief Copy @c src to string @c dst of size @siz.
 * @param dst The destination string.
 * @param src The source string.
 * @param siz The size of the destination string.
 * @returns The total length of the string created.
 * @note If @c retval >= @c siz then a truncation occured.
 *
 * At most siz-1 characters will be copied.  Always terminates with a
 * NULL, unless siz == 0.
 */
static size_t
strlcpy(char *dst, const char *src, size_t siz)
{
  register char       *d = dst;
  register const char *s = src;
  register size_t      n = siz;
  
  /* Copy as many bytes as will fit. */
  if (n != 0 && --n != 0) {
    do {
      if ((*d++ = *s++) == 0) {
        break;
      }
    } while (--n != 0);
  }
  
  /*
   * Not enough room in the destination, add a NULL and traverse the
   * rest of the source.
   */
  if (n == 0) {
    if (siz != 0) {
      *d = '\0';
    }
    
    while (*s++)
      ;
  }
  
  return (s - src - 1);
}

#endif  /* !defined(BSD) */

/**
 * @brief Display program usage to stderr.
 */
static void
usage(void)
{
  fprintf(stderr, "%s\n%s\n",
          "usage: jot [-cnr] [-b word] [-w word] [-s string] [-p precision]",
          "           [reps [begin [end [s]]]]");
  exit(EXIT_FAILURE);
}

/**
 * @brief Put some data to standard output.
 * @param x The data to write.
 * @param notlast Non-zero if this data element is not the last
 *        element of a list.
 * @returns 0 on success; otherwise 1 on failure.
 */
int
putdata(double x, long int notlast)
{
  if (boring) {
    printf("%s", format);
  } else if (longdata && nosign) {
    if (x <= (double)ULONG_MAX && x >= (double)0) {
      printf(format, (unsigned long)x);
    } else {
      return 1;
    }
  } else if (longdata) {
    if (x <= (double)LONG_MAX && x >= (double)LONG_MIN) {
      printf(format, (long)x);
    } else {
      return 1;
    }
  } else if (chardata || (intdata && !nosign)) {
    if (x <= (double)INT_MAX && x >= (double)INT_MIN) {
      printf(format, (int)x);
    } else {
      return 1;
    }
  } else if (intdata) {
    if (x <= (double)UINT_MAX && x >= (double)0) {
      printf(format, (int)x);
    } else {
      return 1;
    }
  } else {
    printf(format, x);
  }
  
  if (notlast != 0) {
    fputs(sepstring, stdout);
  }
  
  return 0;
}

/**
 * @brief Get the precision of a number from a string.
 * @param str The string.
 */
int
getprec(char *str)
{
  char *p;
  char *q;
  
  for (p = str; *p; p++) {
    if (*p == '.') {
      break;
    }
  }
  
  if (!*p) {
    return 0;
  }
  
  for (q = ++p; *p; p++) {
    if (!isdigit(*p)) {
      break;
    }
  }
  
  return p - q;
}

/**
 * @brief Get the format data from the arguments.
 */
void
getformat(void)
{
  char   *p;
  char   *p2;
  int     dot;
  int     hash;
  int     space;
  int     sign;
  int     numbers = 0;
  size_t  sz;
  
  /* No need to bother for boring output. */
  if (boring) {
    return;
  }
  
  for (p = format; *p; p++) {
    if (*p == '%' && *(p + 1) != '%') { /* Look for '%' */
      break;                    /* leave '%%' alone. */
    }
  }
  
  sz = sizeof(format) - strlen(format) - 1;
  
  if (!*p && !chardata) {
    if (snprintf(p, sz, "%%.%df", prec) >= (int)sz) {
      errx(1, "-w word too long.");
    }
  } else if (!*p && chardata) {
    if (strlcpy(p, "%c", sz) >= sz) {
      errx(1, "-w word too long.");
    }
  } else if (!*(p + 1)) {
    if (sz <= 0) {
      errx(1, "-w word too long.");
    }
    
    strcat(format, "%");
  } else {
    /*
     * Allow conversion format specifiers of the form
     * %[#][ ][{+,-}][0-9]*[.[0-9]*]? where ? must be one of
     * [l]{d,i,o,u,x} or {f,e,g,E,G,d,o,x,D,O,U,X,c,u}
     */
    p2  = p++;
    dot = hash = space = sign = numbers = 0;
    
    while (!isalpha(*p)) {
      if (isdigit(*p)) {
        numbers++;
        p++;
      } else if ((*p == '#' && !(numbers | dot | sign | space | hash++)) ||
                 (*p == ' ' && !(numbers | dot | space++))               ||
                 ((*p == '+' || *p == '-') && !(numbers | dot | sign++)) ||
                 (*p == '.' && !(dot++)))
      {
        p++;
      } else {
        goto fmt_broken;
      }
    }
    
    if (*p == 'l') {
      longdata = 1;
      
      if (*++p == 'l') {
        if (p[1] != '\0') {
          p++;
        }
        
        goto fmt_broken;
      }
    }
    
    switch (*p) {
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        intdata = nosign = 1;
        break;
        
      case 'd':
      case 'i':
        intdata = 1;
        break;
        
      case 'D':
        if (!longdata) {
          intdata = 1;
          break;
        }
        
      case 'O':
      case 'U':
        if (!longdata) {
          intdata = nosign = 1;
          break;
        }
        
      case 'c':
        if (!(intdata | longdata)) {
          chardata = 1;
          break;
        }
        
      case 'h':
      case 'n':
      case 'p':
      case 'q':
      case 's':
      case 'L':
      case 'S':
      case '*':
        goto fmt_broken;
        
      case 'f':
      case 'e':
      case 'g':
      case 'E':
      case 'G':
        if (!longdata) {
          break;
        }
        
        /* Fallthrough */
      default:
fmt_broken:
        *++p = '\0';
        errx(1, "illegal or unsupported format '%s'.", p2);
        /* not reached. */
    }
    
    while (*++p) {
      if (*p == '%' && *(p + 1) != '%') {
        errx(1, "too many conversions.");
      } else if (*p == '%' && *(p + 1) == '%') {
        p++;
      } else if (*p == '%' && !*(p + 1)) {
        strcat(format, "%s");
        break;
      }
    }                           /* while */
  }                             /* else */
}

/**
 * @brief Main routine.
 */
int
main(int argc, char **argv)
{
  double        xd;
  double        yd;
  long          id;
  double       *x    = &xd;
  double       *y    = &yd;
  long         *i    = &id;
  unsigned int  mask = 0;
  int           n    = 0;
  int           ch;
  
  while ((ch = getopt(argc, argv, "rb:w:cs:np:")) != -1) {
    switch (ch) {
      case 'r':
        randomize                                  = 1;
        break;
        
      case 'c':
        chardata = 1;
        break;
        
      case 'n':
        nofinalnl = 1;
        break;
        
      case 'b':
        boring = 1;
        /* fallthrough */
        
      case 'w':
        if (strlcpy(format, optarg, sizeof(format)) >= sizeof(format)) {
          errx(1, "-%c word too long.", ch);
        }
        break;
        
      case 's':
        sepstring = optarg;
        break;
        
      case 'p':
        prec = atoi(optarg);
        if (prec <= 0) {
          errx(1, "bad precision value.");
        }
        break;
        
      default:
        usage();
    }                           /* switch(...) */
  }                             /* while (...) */
    
  argc -= optind;
  argv += optind;
  
  switch (argc) {
    case 4:
      if (!is_default(argv[3])) {
        if (!sscanf(argv[3], "%lf", &s)) {
          errx(1, "bad s value: %s", argv[3]);
        }
        
        mask |= 01;
      }
      
    case 3:
      if (!is_default(argv[2])) {
        if (!sscanf(argv[2], "%lf", &ender)) {
          ender = argv[2][strlen(argv[2]) - 1];
        }
        
        mask |= 02;
        
        if (!prec) {
          n = getprec(argv[2]);
        }
      }
      
    case 2:
      if (!is_default(argv[1])) {
        if (!sscanf(argv[1], "%lf", &begin)) {
          begin = argv[1][strlen(argv[1]) - 1];
        }
        
        mask |= 04;
        
        if (!prec) {
          prec = getprec(argv[1]);
        }
        
        if (n > prec) {
          prec = n;
        }
      }
      
    case 1:
      if (!is_default(argv[0])) {
        if (!sscanf(argv[0], "%ld", &reps)) {
          errx(1, "bad reps value: %s", argv[0]);
        }
        
        mask |= 010;
      }
      
      break;
      
    case 0:
      usage();
      
    default:
      errx(1, "too many arguments.  What do you mean by %s?", argv[4]);
  }                             /* switch(...) */
  
  getformat();
  
  while (mask) {
    switch (mask) {
      case 001:
        reps = REPS_DEF;
        mask = 011;
        break;
        
      case 002:
        reps = REPS_DEF;
        mask = 012;
        break;
        
      case 003:
        reps = REPS_DEF;
        mask = 013;
        break;
        
      case 004:
        reps = REPS_DEF;
        mask = 014;
        break;
        
      case 005:
        reps = REPS_DEF;
        mask = 015;
        break;
        
      case 006:
        reps = REPS_DEF;
        mask = 016;
        break;
        
      case 007:
        if (randomize) {
          reps = REPS_DEF;
          mask = 0;
          break;
        }
        
        if (s == 0.0) {
          reps = 0;
          mask = 0;
          break;
        }
        
        reps = (ender - begin + s) / s;
        
        if (reps <= 0) {
          errx(1, "impossible stepsize.");
        }
        
        mask = 0;
        break;
        
      case 010:
        begin = BEGIN_DEF;
        mask  = 014;
        break;
        
      case 011:
        begin = BEGIN_DEF;
        mask  = 015;
        break;
        
      case 012:
        s    = (randomize ? time(NULL) : STEP_DEF);
        mask = 013;
        break;
        
      case 013:
        if (randomize) {
          begin = BEGIN_DEF;
        } else if (reps == 0) {
          errx(1, "must specify begin if reps == 0");
        }
        
        begin = ender - reps * s + s;
        mask  = 0;
        break;
        
      case 014:
        s    = (randomize ? -1.0 : STEP_DEF);
        mask = 015;
        break;
        
      case 015:
        if (randomize) {
          ender = ENDER_DEF;
        } else {
          ender = begin + reps * s - s;
        }
        
        mask = 0;
        break;
        
      case 016:
        if (randomize) {
          s = -1.0;
        } else if (reps == 0) {
          errx(1, "infinite sequences cannot be bounded.");
        } else if (reps == 1) {
          s = 0.0;
        } else {
          s = (ender - begin) / (reps - 1);
        }
        
        mask = 0;
        break;
        
      case 017:
        if (!randomize && s != 0.0) {
          long t = (ender - begin + s) / s;
          
          if (t <= 0) {
            errx(1, "impossible stepsize.");
          }
          
          if (t < reps) {
            reps = t;
          }
        }
        
        mask = 0;
        break;
        
      default:
        errx(1, "bad mask.");
    }                           /* switch (...) */
  }                             /* while (...) */
  
  if (reps == 0) {
    infinity = 1;
  }
  
  if (randomize) {
    *x = (ender - begin) * (ender > begin ? 1 : -1);
    
    for (*i = 1; *i <= reps || infinity; (*i)++) {          
      *y = rand() / (double)UINT32_MAX;
      
      if (putdata(*y * *x + begin, reps - *i)) {
        errx(1, "range error in conversion.");
      }
    }
  } else {
    for (*i = 1, *x = begin; *i <= reps || infinity; (*i)++, *x += s) {
      if (putdata(*x, reps - *i)) {
        errx(1, "range error in conversion.");
      }
    }
  }
  
  if (!nofinalnl) {
    putchar('\n');
  }
  
  exit(EXIT_SUCCESS);
}

/* jot.c ends here */
