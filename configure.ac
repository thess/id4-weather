#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(id4-pi, 1.1, http://www.kitschensync.com/id4-pi)
AM_INIT_AUTOMAKE([1.8 foreign])
AC_CONFIG_HEADERS([config.h])

#AC_DEFINE(PACKAGE, AC_PACKAGE_NAME, [package name])
#AC_DEFINE(VERSION, AC_PACKAGE_VERSION, [package version])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_PROG_RANLIB
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

AC_CONFIG_FILES([Makefile
                 webio/Makefile])
AC_OUTPUT
