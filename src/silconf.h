/*
** $Id: silconf.h $
** Configuration file for SIL
** See Copyright Notice in sil.h
*/


#ifndef silconf_h
#define silconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for SIL
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** SIL ABI (by making the changes here, you ensure that all software
** connected to SIL, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) SIL to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ SIL_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want SIL to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define SIL_USE_C89 */


/*
** By default, SIL on Windows use (some) specific Windows features
*/
#if !defined(SIL_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define SIL_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(SIL_USE_WINDOWS)
#define SIL_DL_DLL	/* enable support for DLL */
#define SIL_USE_C89	/* broadly, Windows is C89 */
#endif


/*
** When Posix DLL ('SIL_USE_DLOPEN') is enabled, the SIL stand-alone
** application will try to dynamically link a 'readline' facility
** for its REPL.  In that case, SIL_READLINELIB is the name of the
** library it will look for those facilities.  If sil.c cannot open
** the specified library, it will generate a warning and then run
** without 'readline'.  If that macro is not defined, sil.c will not
** use 'readline'.
*/
#if defined(SIL_USE_LINUX)
#define SIL_USE_POSIX
#define SIL_USE_DLOPEN		/* needs an extra library: -ldl */
#define SIL_READLINELIB		"libreadline.so"
#endif


#if defined(SIL_USE_MACOSX)
#define SIL_USE_POSIX
#define SIL_USE_DLOPEN		/* MacOS does not need -ldl */
#define SIL_READLINELIB		"libedit.dylib"
#endif


#if defined(SIL_USE_IOS)
#define SIL_USE_POSIX
#define SIL_USE_DLOPEN
#endif


#if defined(SIL_USE_C89) && defined(SIL_USE_POSIX)
#error "Posix is not compatible with C89"
#endif


/*
@@ SILI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define SILI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to SIL must
** use the same configuration.
** ===================================================================
*/

/*
@@ SIL_INT_TYPE defines the type for SIL integers.
@@ SIL_FLOAT_TYPE defines the type for SIL floats.
** SIL should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for SIL_INT_TYPE */
#define SIL_INT_INT		1
#define SIL_INT_LONG		2
#define SIL_INT_LONGLONG	3

/* predefined options for SIL_FLOAT_TYPE */
#define SIL_FLOAT_FLOAT		1
#define SIL_FLOAT_DOUBLE	2
#define SIL_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit SIL) */
#define SIL_INT_DEFAULT		SIL_INT_LONGLONG
#define SIL_FLOAT_DEFAULT	SIL_FLOAT_DOUBLE


/*
@@ SIL_32BITS enables SIL with 32-bit integers and 32-bit floats.
*/
#define SIL_32BITS	0


/*
@@ SIL_C89_NUMBERS ensures that SIL uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(SIL_USE_C89) && !defined(SIL_USE_WINDOWS)
#define SIL_C89_NUMBERS		1
#else
#define SIL_C89_NUMBERS		0
#endif


#if SIL_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if SILI_IS32INT  /* use 'int' if big enough */
#define SIL_INT_TYPE	SIL_INT_INT
#else  /* otherwise use 'long' */
#define SIL_INT_TYPE	SIL_INT_LONG
#endif
#define SIL_FLOAT_TYPE	SIL_FLOAT_FLOAT

#elif SIL_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define SIL_INT_TYPE	SIL_INT_LONG
#define SIL_FLOAT_TYPE	SIL_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define SIL_INT_TYPE	SIL_INT_DEFAULT
#define SIL_FLOAT_TYPE	SIL_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** SIL_PATH_SEP is the character that separates templates in a path.
** SIL_PATH_MARK is the string that marks the substitution points in a
** template.
** SIL_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define SIL_PATH_SEP            ";"
#define SIL_PATH_MARK           "?"
#define SIL_EXEC_DIR            "!"


/*
@@ SIL_PATH_DEFAULT is the default path that SIL uses to look for
** SIL libraries.
@@ SIL_CPATH_DEFAULT is the default path that SIL uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define SIL_VDIR	SIL_VERSION_MAJOR "." SIL_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define SIL_LDIR	"!\\sil\\"
#define SIL_CDIR	"!\\"
#define SIL_SHRDIR	"!\\..\\share\\sil\\" SIL_VDIR "\\"

#if !defined(SIL_PATH_DEFAULT)
#define SIL_PATH_DEFAULT  \
		SIL_LDIR"?.sil;"  SIL_LDIR"?\\init.sil;" \
		SIL_CDIR"?.sil;"  SIL_CDIR"?\\init.sil;" \
		SIL_SHRDIR"?.sil;" SIL_SHRDIR"?\\init.sil;" \
		".\\?.sil;" ".\\?\\init.sil"
#endif

#if !defined(SIL_CPATH_DEFAULT)
#define SIL_CPATH_DEFAULT \
		SIL_CDIR"?.dll;" \
		SIL_CDIR"..\\lib\\sil\\" SIL_VDIR "\\?.dll;" \
		SIL_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define SIL_ROOT	"/usr/local/"
#define SIL_LDIR	SIL_ROOT "share/sil/" SIL_VDIR "/"
#define SIL_CDIR	SIL_ROOT "lib/sil/" SIL_VDIR "/"

#if !defined(SIL_PATH_DEFAULT)
#define SIL_PATH_DEFAULT  \
		SIL_LDIR"?.sil;"  SIL_LDIR"?/init.sil;" \
		SIL_CDIR"?.sil;"  SIL_CDIR"?/init.sil;" \
		"./?.sil;" "./?/init.sil"
#endif

#if !defined(SIL_CPATH_DEFAULT)
#define SIL_CPATH_DEFAULT \
		SIL_CDIR"?.so;" SIL_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ SIL_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows SIL automatically uses "\".)
*/
#if !defined(SIL_DIRSEP)

#if defined(_WIN32)
#define SIL_DIRSEP	"\\"
#else
#define SIL_DIRSEP	"/"
#endif

#endif


/*
** SIL_IGMARK is a mark to ignore all after it when building the
** module name (e.g., used to build the silopen_ function name).
** Typically, the suffix after the mark is the module version,
** as in "mod-v1.2.so".
*/
#define SIL_IGMARK		"-"

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ SIL_API is a mark for all core API functions.
@@ SILLIB_API is a mark for all auxiliary library functions.
@@ SILMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** SIL_BUILD_AS_DLL to get it).
*/
#if defined(SIL_BUILD_AS_DLL)	/* { */

#if defined(SIL_CORE) || defined(SIL_LIB)	/* { */
#define SIL_API __declspec(dllexport)
#else						/* }{ */
#define SIL_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define SIL_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define SILLIB_API	SIL_API
#define SILMOD_API	SIL_API


/*
@@ SILI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ SILI_DDEF and SILI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (SILI_DDEF for
** definitions and SILI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when SIL is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define SILI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define SILI_FUNC	extern
#endif				/* } */

#define SILI_DDEC(dec)	SILI_FUNC dec
#define SILI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ SIL_COMPAT_GLOBAL avoids 'global' being a reserved word
*/
#define SIL_COMPAT_GLOBAL


/*
@@ SIL_COMPAT_5_3 controls other macros for compatibility with SIL 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(SIL_COMPAT_5_3)	/* { */

/*
@@ SIL_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define SIL_COMPAT_MATHLIB

/*
@@ SIL_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (sil_pushunsigned, sil_tounsigned,
** silL_checkint, silL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define SIL_COMPAT_APIINTCASTS


/*
@@ SIL_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define SIL_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define sil_strlen(L,i)		sil_rawlen(L, (i))

#define sil_objlen(L,i)		sil_rawlen(L, (i))

#define sil_equal(L,idx1,idx2)		sil_compare(L,(idx1),(idx2),SIL_OPEQ)
#define sil_lessthan(L,idx1,idx2)	sil_compare(L,(idx1),(idx2),SIL_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined SIL_FLOAT_* / SIL_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ SILI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ SIL_NUMBER_FRMLEN is the length modifier for writing floats.
@@ SIL_NUMBER_FMT is the format for writing floats with the maximum
** number of digits that respects tostring(tonumber(numeral)) == numeral.
** (That would be floor(log10(2^n)), where n is the number of bits in
** the float mantissa.)
@@ SIL_NUMBER_FMT_N is the format for writing floats with the minimum
** number of digits that ensures tonumber(tostring(number)) == number.
** (That would be SIL_NUMBER_FMT+2.)
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ sil_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))


/*
@@ sil_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a sil_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define sil_numbertointeger(n,p) \
  ((n) >= (SIL_NUMBER)(SIL_MININTEGER) && \
   (n) < -(SIL_NUMBER)(SIL_MININTEGER) && \
      (*(p) = (SIL_INTEGER)(n), 1))


/* now the variable definitions */

#if SIL_FLOAT_TYPE == SIL_FLOAT_FLOAT		/* { single float */

#define SIL_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define SILI_UACNUMBER	double

#define SIL_NUMBER_FRMLEN	""
#define SIL_NUMBER_FMT		"%.7g"
#define SIL_NUMBER_FMT_N	"%.9g"

#define l_mathop(op)		op##f

#define sil_str2number(s,p)	strtof((s), (p))


#elif SIL_FLOAT_TYPE == SIL_FLOAT_LONGDOUBLE	/* }{ long double */

#define SIL_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define SILI_UACNUMBER	long double

#define SIL_NUMBER_FRMLEN	"L"
#define SIL_NUMBER_FMT		"%.19Lg"
#define SIL_NUMBER_FMT_N	"%.21Lg"

#define l_mathop(op)		op##l

#define sil_str2number(s,p)	strtold((s), (p))

#elif SIL_FLOAT_TYPE == SIL_FLOAT_DOUBLE	/* }{ double */

#define SIL_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define SILI_UACNUMBER	double

#define SIL_NUMBER_FRMLEN	""
#define SIL_NUMBER_FMT		"%.15g"
#define SIL_NUMBER_FMT_N	"%.17g"

#define l_mathop(op)		op

#define sil_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ SIL_UNSIGNED is the unsigned version of SIL_INTEGER.
@@ SILI_UACINT is the result of a 'default argument promotion'
@@ over a SIL_INTEGER.
@@ SIL_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ SIL_INTEGER_FMT is the format for writing integers.
@@ SIL_MAXINTEGER is the maximum value for a SIL_INTEGER.
@@ SIL_MININTEGER is the minimum value for a SIL_INTEGER.
@@ SIL_MAXUNSIGNED is the maximum value for a SIL_UNSIGNED.
@@ sil_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define SIL_INTEGER_FMT		"%" SIL_INTEGER_FRMLEN "d"

#define SILI_UACINT		SIL_INTEGER

#define sil_integer2str(s,sz,n)  \
	l_sprintf((s), sz, SIL_INTEGER_FMT, (SILI_UACINT)(n))

/*
** use SILI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define SIL_UNSIGNED		unsigned SILI_UACINT


/* now the variable definitions */

#if SIL_INT_TYPE == SIL_INT_INT		/* { int */

#define SIL_INTEGER		int
#define SIL_INTEGER_FRMLEN	""

#define SIL_MAXINTEGER		INT_MAX
#define SIL_MININTEGER		INT_MIN

#define SIL_MAXUNSIGNED		UINT_MAX

#elif SIL_INT_TYPE == SIL_INT_LONG	/* }{ long */

#define SIL_INTEGER		long
#define SIL_INTEGER_FRMLEN	"l"

#define SIL_MAXINTEGER		LONG_MAX
#define SIL_MININTEGER		LONG_MIN

#define SIL_MAXUNSIGNED		ULONG_MAX

#elif SIL_INT_TYPE == SIL_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define SIL_INTEGER		long long
#define SIL_INTEGER_FRMLEN	"ll"

#define SIL_MAXINTEGER		LLONG_MAX
#define SIL_MININTEGER		LLONG_MIN

#define SIL_MAXUNSIGNED		ULLONG_MAX

#elif defined(SIL_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define SIL_INTEGER		__int64
#define SIL_INTEGER_FRMLEN	"I64"

#define SIL_MAXINTEGER		_I64_MAX
#define SIL_MININTEGER		_I64_MIN

#define SIL_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DSIL_32BITS' \
  or '-DSIL_C89_NUMBERS' (see file 'silconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in SIL have only one format item.)
*/
#if !defined(SIL_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ sil_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'sil_strx2number' undefined and SIL will provide its own
** implementation.
*/
#if !defined(SIL_USE_C89)
#define sil_strx2number(s,p)		sil_str2number(s,p)
#endif


/*
@@ sil_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define sil_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ sil_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'sil_number2strx' undefined and SIL will
** provide its own implementation.
*/
#if !defined(SIL_USE_C89)
#define sil_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(SILI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(SIL_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef sil_str2number
#define l_mathop(op)		(sil_Number)op  /* no variant */
#define sil_str2number(s,p)	((sil_Number)strtod((s), (p)))
#endif


/*
@@ SIL_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; SIL will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define SIL_KCONTEXT	ptrdiff_t

#if !defined(SIL_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef SIL_KCONTEXT
#define SIL_KCONTEXT	intptr_t
#endif
#endif


/*
@@ sil_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(sil_getlocaledecpoint)
#define sil_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the SIL API use these macros.
** Define SIL_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(sili_likely)

#if defined(__GNUC__) && !defined(SIL_NOBUILTIN)
#define sili_likely(x)		(__builtin_expect(((x) != 0), 1))
#define sili_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define sili_likely(x)		(x)
#define sili_unlikely(x)	(x)
#endif

#endif


#if defined(SIL_CORE) || defined(SIL_LIB)
/* shorter names for SIL's own use */
#define l_likely(x)	sili_likely(x)
#define l_unlikely(x)	sili_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ SIL_NOCVTN2S/SIL_NOCVTS2N control how SIL performs some
** coercions. Define SIL_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define SIL_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define SIL_NOCVTN2S */
/* #define SIL_NOCVTS2N */


/*
@@ SIL_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
/* #define SIL_USE_APICHECK */

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile SIL and when you compile code that links to
** SIL).
** =====================================================================
*/

/*
@@ SILI_MAXSTACK limits the size of the SIL stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop SIL from consuming unlimited stack
** space and to reserve some numbers for pseudo-indices.
** (It must fit into max(int)/2.)
*/
#if 1000000 < (INT_MAX / 2)
#define SILI_MAXSTACK		1000000
#else
#define SILI_MAXSTACK		(INT_MAX / 2u)
#endif


/*
@@ SIL_EXTRASPACE defines the size of a raw memory area associated with
** a SIL state with very fast access.
** CHANGE it if you need a different size.
*/
#define SIL_EXTRASPACE		(sizeof(void *))


/*
@@ SIL_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/
#define SIL_IDSIZE	60


/*
@@ SILL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define SILL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(sil_Number)))


/*
@@ SILI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define SILI_MAXALIGN  sil_Number n; double u; void *s; sil_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

