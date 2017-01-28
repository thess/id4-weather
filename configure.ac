#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(id4-pi, 1.2, http://www.kitschensync.com/id4-pi)
AM_INIT_AUTOMAKE([1.8 foreign])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create],,
        [AC_MSG_ERROR([POSIX thread support required],1)])
AC_CHECK_LIB([rt], [mq_open],,
        [AC_MSG_ERROR([librt required],1)])
AC_CHECK_LIB([curl], [curl_easy_init],,
        [AC_MSG_ERROR([libcurl required],1)])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h memory.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime localtime_r memset mkdir select socket strchr strerror strrchr strstr])

ID4001_PPFLAGS="-DLINUX -D_GNU_SOURCE"
ID4001_WFLAGS="-Wno-multichar"
AC_SUBST(ID4001_PPFLAGS)
AC_SUBST(ID4001_WFLAGS)

# Silence warning: ar: 'u' modifier ignored since 'D' is the default
AC_SUBST(ARFLAGS, [cr])

AC_CONFIG_FILES([Makefile
                 webio/Makefile])
AC_OUTPUT
