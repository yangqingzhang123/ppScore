#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([common.mk])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([Server/query_processor.hpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ljson':
AC_CHECK_LIB([json], [main])
# FIXME: Replace `main' with a function in `-lmjcommon':
AC_CHECK_LIB([mjcommon], [main])
# FIXME: Replace `main' with a function in `-lmjmysql':
AC_CHECK_LIB([mjmysql], [main])
# FIXME: Replace `main' with a function in `-lmysqlclient':
AC_CHECK_LIB([mysqlclient], [main])
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h malloc.h netinet/in.h stdlib.h sys/socket.h sys/time.h sys/timeb.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64 -std=c++0x"

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_CHECK_FUNCS([gettimeofday inet_ntoa memset pow select socket sqrt tzset])

AC_CONFIG_FILES([Makefile
                 Server/Makefile
                 master/Makefile])
AC_OUTPUT
