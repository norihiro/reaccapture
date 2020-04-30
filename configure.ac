AC_INIT([reaccapture], [0.9.0])

AC_CONFIG_HEADERS([reaccapture-config.h])
# TODO: what is the proper declaration of AM_INIT_AUTOMAKE?
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_CXX

AC_CHECK_HEADERS([pthread.h],
	AC_CHECK_LIB(pthread, pthread_create, LIBS="$LIBS -lpthread",
		AC_MSG_ERROR([pthread.h found but there is no pthread library to make use of])
	),
	AC_MSG_ERROR([no pthread.h header header file found])
)

AC_CHECK_HEADERS([pcap.h],
	AC_CHECK_LIB(pcap, pcap_open_live, LIBS="$LIBS -lpcap",
		AC_MSG_ERROR([pcap.h found but there is no pcap library to make use of])
	),
	AC_MSG_ERROR([no pcap.h header header file found])
)

AC_OUTPUT

