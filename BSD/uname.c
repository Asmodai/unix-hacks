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
 * This uses the kernel namelist to get at the kernel version, thus it
 * absolutely *must* be setuid, sorry.
 */
/* }}} */

/**
 * @file uname.c
 * @author Paul Ward
 * @brief `uname` for old BSD releases.
 */

#include <stdio.h>
#include <ctype.h>
#include <nlist.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>

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
char version[255];

/* Various buffers. */
char *progname;

/* Option flags. */
int uFlags = 0;

#define UTS_SYSNAME   1                 /* System name, e.g. BSD. */
#define UTS_NODENAME  2                 /* Node name.             */
#define UTS_RELEASE   4                 /* Release, e.g. 4.3.     */
#define UTS_VERSION   8                 /* Kernel version.        */
#define UTS_MACHINE   16                /* Machine type.          */
#define UTS_ARCH      32                /* Architecture.          */

/* Namelist indices. */
#define KERNEL_VERSION    0

/* Kernel namelists. */
struct nlist knl[] = {
  { "_version" },
  { 0 }
};

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
 * Get version from kernel namelist and compute version and release.
 */
get_kernel_version()
{
  size_t  idx  = 0;
  size_t  len  = 255;
  int     kmem = -1;
  char   *p    = NULL;

  extern char *index();

  memset(version, 0, len);

  nlist("/vmunix", knl);
  if (knl[0].n_type == 0) {
    fprintf(stderr, "No /vmunix namelist!\n");
    exit(1);
  }

  if ((kmem = open("/dev/kmem", 0)) < 0) {
    perror("opening /dev/kmem");
    exit(1);
  }

  lseek(kmem, (long)knl[KERNEL_VERSION].n_value, L_SET);
  read(kmem, &version, len);

  if ((p = index(version, '\n')) != NULL) {
    *p = ' ';
  }

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
  uFlags = 0;

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
  get_hostname();
  get_kernel_version();

  /* Print what needs to be printed. */
  print_element(UTS_SYSNAME,  sysname);
  print_element(UTS_NODENAME, nodename);
  print_element(UTS_RELEASE,  release);
  print_element(UTS_VERSION,  version);
  print_element(UTS_MACHINE,  MACHINE);
  print_element(UTS_ARCH,     MACHINE);

  return 0;
}

/* uname.c ends here. */
