#ifndef NEWSBEUTER_CONFIG__H
#define NEWSBEUTER_CONFIG__H

#define PACKAGE				"newsbeuter"
#define PROGRAM_NAME			PACKAGE
#define REAL_VERSION "2.10"

#ifdef GIT_HASH
#define PROGRAM_VERSION			REAL_VERSION "-" GIT_HASH
#else
#define PROGRAM_VERSION			REAL_VERSION
#endif

#define PROGRAM_URL			"http://www.newsbeuter.org/"

#define NEWSBEUTER_PATH_SEP			"/"
#define NEWSBEUTER_CONFIG_SUBDIR	".newsbeuter"
#define NEWSBEUTER_SUBDIR_XDG		"newsbeuter"

#include <libintl.h>
#include <locale.h>

#ifdef _
#undef _
#endif

#define _(string) gettext(string)

#ifdef _s
#undef _s
#endif

#define _s(msg) std::string(gettext(msg))

/* #define NDEBUG */ // only enable this #define if you want to disable all debug logging.

#endif
