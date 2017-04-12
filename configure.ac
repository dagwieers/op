#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([op],[1.34],[alec@swapoff.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_FILES([Makefile op.list])
AM_MAINTAINER_MODE
AC_CANONICAL_BUILD
dnl ac_cv_build=x86_64-unknown-linux-gnu
AC_USE_SYSTEM_EXTENSIONS

# Checks for programs.
AC_PROG_CC
AC_PROG_LEX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Feature checks.
auto_detect_auth=no
with_pam=${with_pam:-yes}
AC_ARG_WITH([pam],
	AS_HELP_STRING([--with-pam],
		[use PAM for authentication (yes)])])
AS_IF([test "_${with_pam}" = _yes], [
	AC_CHECK_LIB([pam], [pam_start], [
		AC_DEFINE(USE_PAM, [], [Use PAM for authentication])
		LIBS="$LIBS -lpam"
	], [
		AC_MSG_WARN([Can't find pam_start() in libpam, trying shadow support])
		auto_detect_auth=yes
		with_pam=no
		# Try shadow if PAM failed
		with_shadow=yes
	])
])

with_shadow=${with_shadow:-no}
AC_ARG_WITH([shadow],
	AS_HELP_STRING([--with-shadow],
		[use shadow file for authentication
                          (default if PAM not found)])])
AS_IF([test "_${with_shadow}" = _yes], [
	AS_IF([test "_${with_pam}" = _yes], [
		AC_MSG_WARN([Can't enable PAM and shadow support, shadow disabled])
	], [
		AS_IF([test "_${auto_detect_auth}" = _yes -a \
			    ! -e /etc/shadow], [
			AC_MSG_WARN([Failed to find shadow support while auto-detecting authentication mechanisms, using basic crypt])
		], [
			AS_IF([test ! -e /etc/shadow], [
				AC_MSG_WARN([Shadow support enabled but /etc/shadow does not exist, continuing anyway])
			])
			AC_DEFINE(USE_SHADOW, [], [Use shadow file for authentication])
		])
	])
])

enable_xauth=${enable_xauth:-no}
AC_ARG_ENABLE([xauth],
	AS_HELP_STRING([--enable-xauth=<xauth>],
		[enable xauth support and specify xauth binary])])
AS_IF([test "_${enable_xauth}" != _no], [
	EXTRA_CPPFLAGS="$EXTRA_CPPFLAGS -DXAUTH=\\\"$enable_xauth\\\""
])

# Checks for libraries.
AC_CHECK_LIB([bsd], [strlcat], [
	AC_DEFINE(HAVE_LIBBSD, [1],
		[Define to 1 if you have the `bsd' library (-lbsd)])
	LIBS="$LIBS -lbsd"
])
AC_CHECK_LIB([fl], [yywrap], [], AC_CHECK_LIB([l], [yywrap], []))

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([limits.h netdb.h shadow.h stdlib.h string.h \
		  sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gethostname getpass getspnam memset regcomp \
		strchr strcspn strdup strrchr strtol vsnprintf])
AC_CHECK_FUNC([crypt],
	[], [AC_CHECK_LIB([crypt], [crypt], [],
	[AC_MSG_ERROR(Can't find crypt())])])
AC_CHECK_FUNC([regfree],
	[AC_DEFINE(HAVE_REGEX, [1],
	 [Define to 1 if you have the `regex' functions.])])
AC_CHECK_FUNC([strlcat],
	[AC_DEFINE(HAVE_STRLCAT, [1],
	 [Define to 1 if you have the `strlcat' function.])])
AC_CHECK_FUNC([strlcpy],
	[AC_DEFINE(HAVE_STRLCPY, [1],
	 [Define to 1 if you have the `strlcpy' function.])])
AC_CHECK_FUNC([vsnprintf],
	[AC_DEFINE(HAVE_VSNPRINTF, [1],
	[Define to 1 if you have the `vsnprintf' function.])])

# Custom checks.
AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE([int main(int, char**){int i = openlog(); return 0;}])],
	[OPENLOG_VOID=0],
	[OPENLOG_VOID=1])
AC_DEFINE_UNQUOTED([OPENLOG_VOID],
	[${OPENLOG_VOID}],
	[Does openlog() return void?])

# Check for compiler options.
dnl AC_PROG_CC_STDC
dnl ac_cv_prog_cc_stdc=-std=gnu99
dnl ac_cv_c_compiler_gnu=yes

AX_COMPILER_VENDOR
dnl ax_cv_c_compiler_vendor=gnu
dnl AX_COMPILER_VERSION
dnl ax_cv_c_compiler_version=4.4.7

AX_CFLAGS_WARN_ALL
dnl -Waddress
dnl -Warray-bounds
dnl -Wc++0x-compat
dnl -Wchar-subscripts
dnl -Wimplicit-int
dnl -Wimplicit-function-declaration
dnl -Wcomment
dnl -Wformat
dnl -Wmain
dnl -Wmissing-braces
dnl -Wnonnull
dnl -Wparentheses
dnl -Wpointer-sign
dnl -Wreorder
dnl -Wreturn-type
dnl -Wsequence-point
dnl -Wsign-compare
dnl -Wstrict-aliasing
dnl -Wstrict-overflow=1
dnl -Wswitch
dnl -Wtrigraphs
dnl -Wuninitialized
dnl -Wunknown-pragmas
dnl -Wunused-function
dnl -Wunused-label
dnl -Wunused-value
dnl -Wunused-variable
dnl -Wvolatile-register-var

AS_IF([test "_${ax_cv_c_compiler_vendor}" = _ibm], [
	# Stack Smashing Protection
	AX_CFLAGS_AIX_OPTION([-qstackprotect], [EXTRA_CFLAGS])
])

AS_IF([test "_${ax_cv_c_compiler_vendor}" = _gnu], [
	dnl rhel  3.x  gcc 3.2.3 ld 2.14 i686-pc-linux-gnu
	dnl rhel  4.x  gcc 3.4.6 ld 2.15
	dnl rhel  5.x  gcc 4.1.2 ld 2.17
	dnl rhel  6.x  gcc 4.4.7 ld 2.20 x86_64-unknown-linux-gnu
	dnl rhel  7.x  gcc 4.8.5 ld 2.25 x86_64-unknown-linux-gnu
	dnl aix   5.2  gcc 3.3.2 ld aix  powerpc-ibm-aix5.2.0.0
	dnl hpux 11.00 gcc 3.2   ld hp   hppa2.0w-hp-hpux11.00
	dnl hpux 11.11 gcc 4.1.1 ld hp   hppa2.0w-hp-hpux11.11
	dnl sunos 5.9  gcc 3.4.6 ld sun  sparc-sun-solaris2.9

dnl https://www.owasp.org/index.php/C-Based_Toolchain_Hardening
dnl http://connect.ed-diamond.com/GNU-Linux-Magazine/GLMFHS-076/Les-options-de-securite-de-gcc
dnl https://wiki.debian.org/Hardening
dnl https://wiki.ubuntu.com/ToolChain/CompilerFlags
dnl https://wiki.ubuntu.com/SecurityTeam/KnowledgeBase/BuiltPIE
dnl https://fedoraproject.org/wiki/Security_Features_Matrix
dnl https://fedoraproject.org/wiki/Hardened_Packages
dnl https://wiki.gentoo.org/wiki/Hardened/Toolchain
dnl https://wiki.archlinux.org/index.php/DeveloperWiki:Security

	AS_IF([test "_${USE_MAINTAINER_MODE}" = _no], [
		enable_compile_warnings=${enable_compile_warnings:-no}
	], [
		enable_Werror=${enable_Werror:-no}
	])

	dnl AX_IS_RELEASE([git-directory])

	AC_MSG_NOTICE([USE_MAINTAINER_MODE=$USE_MAINTAINER_MODE])
	AC_MSG_NOTICE([ax_is_release=$ax_is_release])
	AC_MSG_NOTICE([enable_compile_warnings=$enable_compile_warnings])

	AX_COMPILER_FLAGS
	dnl WARN_CFLAGS = -fno-strict-aliasing
	dnl -Wall
	dnl -Wextra
	dnl -Wundef
	dnl -Wnested-externs
	dnl -Wwrite-strings
	dnl -Wpointer-arith
	dnl -Wmissing-declarations
	dnl -Wmissing-prototypes
	dnl -Wstrict-prototypes
	dnl -Wredundant-decls
	dnl -Wno-unused-parameter
	dnl -Wno-missing-field-initializers
	dnl -Wdeclaration-after-statement
	dnl -Wformat=2
	dnl -Wold-style-definition
	dnl -Wcast-align
	dnl -Wformat-nonliteral
	dnl -Wformat-security
	dnl -Wsign-compare
	dnl -Wstrict-aliasing
	dnl -Wshadow
	dnl -Winline
	dnl -Wpacked
	dnl -Wmissing-format-attribute
	dnl -Wmissing-noreturn
	dnl -Winit-self
	dnl -Wmissing-include-dirs
	dnl -Wunused-but-set-variable
	dnl -Warray-bounds
	dnl -Wimplicit-function-declaration
	dnl -Wreturn-type
	dnl -Wswitch-enum
	dnl -Wswitch-default
	dnl -Werror
	dnl -Wno-suggest-attribute=format
	dnl -Wno-error=unused-parameter
	dnl -Wno-error=missing-field-initializers
	dnl WARN_LDFLAGS = -Wl,--no-as-needed -Wl,--fatal-warnings
	dnl WARN_SCANNERFLAGS = --warn-all

	# Enables many warnings (despite their names, all and extra do not turn on all warnings)
	AX_APPEND_COMPILE_FLAGS([-pedantic], [EXTRA_CPPFLAGS])

	# MAINTAINER_MODE
	AX_APPEND_COMPILE_FLAGS([-W], [EXTRA_CPPFLAGS])
	AX_APPEND_COMPILE_FLAGS([-Wextra], [EXTRA_CPPFLAGS])
	AX_APPEND_COMPILE_FLAGS([-Wunused], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wclobbered], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wempty-body], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wignored-qualifiers], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wmissing-field-initializers], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wmissing-parameter-type], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wold-style-declaration], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Woverride-init], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wtype-limits], [EXTRA_CPPFLAGS])
	dnl AX_APPEND_COMPILE_FLAGS([-Wunused-parameter], [EXTRA_CPPFLAGS])

	# Warn for implicit conversions that may alter a value (includes -Wsign-conversion)
	AX_APPEND_COMPILE_FLAGS([-Wconversion], [EXTRA_CPPFLAGS])

	# Warn for implicit conversions that may change the sign of an integer value, such
	# as assigning a signed integer to an unsigned integer (-1 > 1 after promotion!)
	dnl AX_APPEND_COMPILE_FLAGS([-Wsign-conversion], [EXTRA_CPPFLAGS])

	# Warn for a pointer cast to a type which has a different size, causing an invalid
	# alignment and subsequent bus error on ARM processors
	AX_APPEND_COMPILE_FLAGS([-Wcast-align], [EXTRA_CPPFLAGS])

	# Increases warnings related to possible security defects, including incorrect format
	# specifiers (includes -Wformat -Wformat-nonliteral -Wformat-security -Wformat-y2k)
	AX_APPEND_COMPILE_FLAGS([-Wformat=2], [EXTRA_CPPFLAGS])

	# Warn if a global function is defined without a prototype or declaration
	dnl AX_APPEND_COMPILE_FLAGS([-Wmissing-prototypes], [EXTRA_CPPFLAGS])
	AX_APPEND_COMPILE_FLAGS([-Wmissing-declarations], [EXTRA_CPPFLAGS])

	# Warn if a function is declared or defined without specifying the argument types
	AX_APPEND_COMPILE_FLAGS([-Wstrict-prototypes], [EXTRA_CPPFLAGS])

	# Warn about optimizations taken due to [undefined] signed integer overflow assumptions
	AX_APPEND_COMPILE_FLAGS([-Wstrict-overflow], [EXTRA_CPPFLAGS]) # gcc 4.2

	# Enable exception handling.  Generates extra code needed to propagate exceptions
	AX_APPEND_COMPILE_FLAGS([-fexceptions], [EXTRA_CFLAGS])

	# Enable AddressSanitizer, a fast memory error detector. Memory access instructions
	# will be instrumented to help detect heap, stack, and global buffer overflows;
	# as well as use-after-free bugs
	AX_APPEND_COMPILE_FLAGS([-fsanitize=address], [EXTRA_CFLAGS]) # gcc 4.8
	AX_APPEND_COMPILE_FLAGS([-static-libasan], [EXTRA_CFLAGS]) # gcc 4.8

	# FORTIFY_SOURCE / buffer overflow
	AX_APPEND_COMPILE_FLAGS(['-O1 -D_FORTIFY_SOURCE=2'], [EXTRA_CPPFLAGS]) # gcc 4.1

	dnl # too many warning: frame size too large for reliable stack checking
	dnl AX_APPEND_COMPILE_FLAGS([-fstack-check], [EXTRA_CFLAGS])

	# Stack Smashing Protection
	# Improves stack layout and adds a guard to detect stack based buffer overflows
	AS_CASE([${ac_cv_build}],
		dnl hpux : not supported for this target
		[*hpux*], [],
		[
		dnl too many warning: not protecting local variables: variable length buffer
		dnl AX_APPEND_COMPILE_FLAGS([-Wstack-protector], [EXTRA_CPPFLAGS])
		AX_APPEND_COMPILE_FLAGS([-fstack-protector-strong], [EXTRA_CFLAGS]) # gcc 4.9

		AS_IF([test "_${ax_cv_check_cflags___fstack_protector_strong}" = _no], [
			AX_APPEND_COMPILE_FLAGS([-fstack-protector-all], [EXTRA_CFLAGS]) # gcc 3.x
		])

		AX_APPEND_COMPILE_FLAGS(['--param ssp-buffer-size=4'], [EXTRA_CFLAGS])
	])

	# Data Execution Prevention (DEP). ELF headers are marked with PT_GNU_STACK and PT_GNU_HEAP
	AX_APPEND_LINK_FLAGS([-Wl,-z,noexecstack], [EXTRA_LDFLAGS]) # ld 2.14
	AX_APPEND_LINK_FLAGS([-Wl,-z,noexecheap], [EXTRA_LDFLAGS]) # ld 2.14

	# RELocation Read-Only
	# Helps remediate Global Offset Table (GOT) attacks on executables
	# Helps remediate Procedure Linkage Table (PLT) attacks on executables
	AX_APPEND_LINK_FLAGS([-Wl,-z,relro], [EXTRA_LDFLAGS]) # ld 2.15
	AX_APPEND_LINK_FLAGS([-Wl,-z,now], [EXTRA_LDFLAGS]) # ld 2.15

	AS_CASE([${ac_cv_build}],
		dnl aix : unrecognized option -pie
		[*-aix*], [],
		dnl solaris is buggy
		[*-solaris*], [],
		[
		# Position Independent Executable (ASLR). Used for programs.
		# Both -fPIE (compiler) and -pie (linker) are required
		AX_APPEND_COMPILE_FLAGS([-fPIE], [EXTRA_CFLAGS]) # gcc 3.x
		AX_APPEND_LINK_FLAGS([-pie], [EXTRA_LDFLAGS]) # ld 2.16
	])

	CFLAGS=`echo "$CFLAGS" | sed 's/-O2//'` 
])

AC_SUBST([EXTRA_CPPFLAGS])
AC_SUBST([EXTRA_CFLAGS])
AC_SUBST([EXTRA_LDFLAGS])

AC_OUTPUT
