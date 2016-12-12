/*
 * uname.c --- `uname` for old BSD releases.
 *
 * Copyright (c) 2016 Paul Ward <asmodai@gmail.com>
 *
 * Author:     Paul Ward <asmodai@gmail.com>
 * Maintainer: Paul Ward <asmodai@gmail.com>
 * Created:    11 Dec 2016 14:07:34
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

/**
 * @file uname.c
 * @author Paul Ward
 * @brief `uname` for old BSD releases.
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/types.h>

/*
 * This relies on a kernel to be configured and built.
 */
#include <vers.c>

/*
 * Guess the system name based on what preprocessor macros have been
 * defined by various headers.
 */
static char *sysname =
#if defined(ULTRIX)
  "ULTRIX"
#else
# if defined(BSD)
  "BSD"
# else
  "UNKNOWN"
# endif
#endif
  ;

/* We get the node name and release at run time. */
char nodename[MAXHOSTNAMELEN];
char release[255];

/* Various buffers. */
char *progname;
char *nicevers;

/* Option flags. */
int uFlags = 0;

#define UTS_SYSNAME   1                 /* System name, e.g. BSD. */
#define UTS_NODENAME  2                 /* Node name.             */
#define UTS_RELEASE   4                 /* Release, e.g. 4.3.     */
#define UTS_VERSION   8                 /* Kernel version.        */
#define UTS_MACHINE   16                /* Machine type.          */
#define UTS_ARCH      32                /* Architecture.          */

/*
 * Display usage information and exit.
 */
usage()
{
  fprintf(stderr, "usage: %s [-amnprsv]\n", progname);
  exit(1);
}

/*
 * Print an element if required.
 */
print_element(mask, element)
     unsigned int  mask;                /* Element mask. */
     char         *element;             /* String to display. */
{
  if (uFlags & mask) {
    uFlags &= ~mask;
    printf("%s%c", element, uFlags ? ' ' : '\n');
  }
}

/*
 * Get the system hostname.
 */
get_hostname()
{
  if (gethostname(nodename, sizeof(nodename))) {
    perror("gethostname");
    exit(1);
  }
}

/*
 * Strip any newlines from the kernel version string.
 */
get_nice_version()
{
  size_t idx = 0;
  size_t len = strlen(version);

  nicevers = (char *)malloc(sizeof(char) * len);
  if (nicevers == NULL) {
    perror("malloc");
    exit(1);
  }

  for (idx = 0; idx < (len - 1); idx++) {
    if (version[idx] == '\0') {
      nicevers[idx] = '\0';
      break;
    }

    if (version[idx] != '\n') {
      nicevers[idx] = version[idx];
    }
  }
}

/*
 * Get the system version.
 *
 * NB:  This has to parse the version string from `vers.c`.
 */
get_system()
{
  size_t idx = 0;
  size_t len = strlen(version);

  for (idx = 0; idx < len; idx++) {
    
    /* Release version should match \d.\d */
    if (isdigit(version[idx])     &&
        version[idx + 1] == '.'   &&
        isdigit(version[idx + 2]))
    {
      sprintf(release, "%c.%c\0", version[idx], version[idx + 2]);
      break;
    }
  }
}

/*
 * Main routine.
 */
main(argc, argv)
     int    argc;                       /* Argument count.       */
     char **argv;                       /* Argument value array. */
{
  progname = argv[0];
  uFlags   = 0;

  while (--argc > 0 && **(++argv) == '-') {
    while (*(++(*argv))) {
      switch (**argv) {
        case 'h':
          usage();
          break;

        case 'a':
          uFlags = (UTS_SYSNAME  |
                    UTS_NODENAME |
                    UTS_RELEASE  |
                    UTS_ARCH     |
                    UTS_VERSION  |
                    UTS_MACHINE);
          break;

        case 'm': uFlags |= UTS_MACHINE;  break;
        case 'n': uFlags |= UTS_NODENAME; break;
        case 'p': uFlags |= UTS_ARCH;     break;
        case 'r': uFlags |= UTS_RELEASE;  break;
        case 's': uFlags |= UTS_SYSNAME;  break;
        case 'v': uFlags |= UTS_VERSION;  break;

        default:
          usage();
          break;
      }
    }
  }

  /* If we don't have any flags, default to system name. */
  if (uFlags == 0) {
    uFlags = UTS_SYSNAME;
  }

  /* Get information. */
  get_nice_version();
  get_hostname();
  get_system();

  /* Print what needs to be printed. */
  print_element(UTS_SYSNAME,  sysname);
  print_element(UTS_NODENAME, nodename);
  print_element(UTS_RELEASE,  release);
  print_element(UTS_VERSION,  nicevers);
  print_element(UTS_MACHINE,  MACHINE);
  print_element(UTS_ARCH,     MACHINE);

  return 0;
}

/* uname.c ends here. */
