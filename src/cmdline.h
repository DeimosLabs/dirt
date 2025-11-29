
/* 
 * Take care of these command line shenanigans
 * ONCE AND FOR ALL.
 * Without wasting "too much" memory and/or CPU cycles.
 * 
 * Since most of the same code can be used for parsing plain-text config
 * files, i'm also including the functionality for that in here too. Same with
 * linklist stuff (double-linked list with variable size for each element) and
 * a fancy color debug wrapper for printf.
 * 
 * ADDED: circa 2015-2016 (?)
 *
 * Also see below for C++ wrapper classes, example usage etc.
 *
 * #define CMDLINE_ANSI before #including this file if you want ANSI escape
 * sequences for colors defined. You can then easily create a
 *   debug (char *fmt, ...)
 * macro with something like:
 *
 * #define debug(...) cmdline_debug(stderr,SOME_COLOR_STRING,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
 * (see below for __FUNC__ being defined to __PRETTY_FUNCTION__ - gcc thing)
 *
 * ...where SOME_COLOR_STRING can be any ANSI color, see below.
 *
 * ADDED: may 2021:
 * Instead of building this separately and linking against it, you can just
 * #include this file from a .c / .cpp file after #defining
 * CMDLINE_IMPLEMENTATION (entire C code is copied and #ifdef'd below)
 * Non-conventional, but avoids a ton of hassle when porting to many platforms.
 * (note: do this in ONLY ONE file of your project!)
 * 
 * Example inclusion from .c file:

#define CMDLINE_IMPLEMENTATION // This should only be in ONE implementation file!!
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)

 * 
 *   - Éric. (delt)
 */

#ifndef __CMDLINE_H
#define __CMDLINE_H
#define __IN_CMDLINE_H

#define CMDLINE_DEBUG

// define this to get CP and BP debugging macros
#ifdef CMDLINE_DEBUG
#define __FUNC__ __PRETTY_FUNCTION__
#ifndef CMDLINE_CP_BP
#define CMDLINE_CP_BP
#endif
#ifndef CMDLINE_ANSI
#define CMDLINE_ANSI
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* some useful uses of our debug function for checkpoints, breakpoints etc. */
#ifdef CMDLINE_CP_BP
#ifndef CP
#define CP {debug("\x1B[1;37m____CHECKPOINT____\x1B[0;37m");}
#endif
#ifndef BP
#define BP {debug("\x1B[1;37m____BREAKPOINT____\x1B[0;37m");getc(stdin);}
#endif
#endif

#if defined _WIN32 || defined _WIN64
#ifndef WIN32
#define WIN32
#endif
#endif

#define CMDLINE_MAX_OPTS 4096 /* this excludes 'extra' arguments, such as filenames etc */
#define CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH 32
#define CMDLINE_MAX_LENGTH 1024

#ifdef C89
#define strtof strtod
#define strcasecmp strcmp
#define strncasecmp strncmp
#define random rand
#define srandom srand
#endif

//#ifndef bool // ???
#ifndef __cplusplus
#ifdef bool
#undef bool
#endif
#define bool char
#endif
//#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* synonyms for cmdline types */
#define CMDLINE_TYPE_NONE          CMD_NONE
#define CMDLINE_TYPE_VOID          CMD_VOID
#define CMDLINE_TYPE_STRING        CMD_STRING
#define CMDLINE_TYPE_STRLIST       CMD_STRLIST
#define CMDLINE_TYPE_BOOL          CMD_BOOL
#define CMDLINE_TYPE_INT           CMD_INT
#define CMDLINE_TYPE_LONGINT       CMD_LONGINT
#define CMDLINE_TYPE_FLOAT         CMD_FLOAT
#define CMDLINE_TYPE_COUNT         CMD_COUNT

#define CMDLINE_TYPE_NONE_CFG      CMD_NONE_CFG
#define CMDLINE_TYPE_VOID_CFG      CMD_VOID_CFG
#define CMDLINE_TYPE_STRING_CFG    CMD_STRING_CFG
//#define CMDLINE_TYPE_STRLIST_CFG   CMD_STRLIST_CFG // not written to config files (yet)
#define CMDLINE_TYPE_BOOL_CFG      CMD_BOOL_CFG
#define CMDLINE_TYPE_INT_CFG       CMD_INT_CFG
#define CMDLINE_TYPE_LONGINT_CFG   CMD_LONGINT_CFG
#define CMDLINE_TYPE_FLOAT_CFG     CMD_FLOAT_CFG
#define CMDLINE_TYPE_COUNT_CFG     CMD_COUNT_CFG

#define CMDLINE_WRITE_TO_CFG_FLAG  0x4000

/* constants for opts overwrite mode */
#define CMDLINE_FILE_OPTS     (1 <<  0)
#define CMDLINE_CMDLINE_OPTS  (1 <<  1)
#define CMDLINE_ENV_OPTS      (1 <<  2)
#define CMDLINE_NO_OPTS       0
#define CMDLINE_ALL_OPTS      7

/* for cmdline_write_file and c_cmdline::write_file, when file already exists */
enum {
  CMDLINE_ABORT,
  CMDLINE_OVERWRITE,
  CMDLINE_APPEND
};

/* data types: CMDLINE_TYPE_NONE is used to mark the end of the list.
 * CMDLINE_TYPE_COUNT is a value that increments each time it appears on the 
 * command line. Mostly useful for verbose or debug flags. */

enum {
  CMDLINE_TYPE_NONE,
  CMDLINE_TYPE_VOID,
  CMDLINE_TYPE_STRING,
  CMDLINE_TYPE_STRLIST,
  CMDLINE_TYPE_BOOL,
  CMDLINE_TYPE_INT,
  CMDLINE_TYPE_LONGINT,
  CMDLINE_TYPE_FLOAT,
  CMDLINE_TYPE_COUNT,
  
  CMDLINE_TYPE_NONE_CFG = CMDLINE_WRITE_TO_CFG_FLAG,
  CMDLINE_TYPE_VOID_CFG,
  CMDLINE_TYPE_STRING_CFG,
  //CMDLINE_TYPE_STRLIST_CFG = CMDLINE_WRITE_TO_CFG_FLAG + 3,
  CMDLINE_TYPE_BOOL_CFG,
  CMDLINE_TYPE_INT_CFG,
  CMDLINE_TYPE_LONGINT_CFG,
  CMDLINE_TYPE_FLOAT_CFG,
  CMDLINE_TYPE_COUNT_CFG
};

/* error codes for each option */
enum {
  CMDLINE_ERROR_NOERROR = 0,
  CMDLINE_ERROR_BOUNDS,
  CMDLINE_ERROR_NAN,
  CMDLINE_ERROR_NOTBOOL,
  CMDLINE_ERROR_NOARG
};

enum {
  CMDLINE_MODE_CMDLINE,
  CMDLINE_MODE_ENV,
  CMDLINE_MODE_FILE
};


typedef struct s_linklist t_linklist;
typedef struct s_linkitem t_linkitem;
typedef struct s_argsetup t_argsetup;
typedef struct s_opt t_opt;
typedef struct s_cmdline t_cmdline;

typedef int (cmpfunc_void) (void *data1, void *data2);

struct s_linkitem {
  size_t size;
  int index;
  bool reserved0;
  bool reserved1;
  bool reserved2;
  bool is_copy;
  t_linkitem *next;
  t_linkitem *prev;
  void *data;
  void *reserved3;
  uint64_t userdata0;
  uint64_t userdata1;
};

struct s_linklist {
  int count;
  bool auto_unref_items; /* automatically free items that weren't copied */
  bool sorted;
  t_linkitem *first;
  t_linkitem *last;
  t_linkitem *last_accessed;
  
  void *(*malloc_func) (size_t s);
  void (*free_func) (void *ptr);
};

struct s_argsetup {
  char shortname;
  const char *longname;
  int type;
  float min;
  float max;
};

struct s_opt {
  bool unknown;
  int lookup_count;
  bool from_cmdline;
  bool from_env;
  bool from_file;
  bool write_to_file;
  int index;
  int argv_index;
  char shortname;
  char longname [CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH];
  //char *longname;
  int longname_length;
  //char *value;
  int value_length;
  int type;
  int errorcode;
  float min;
  float max;
  int num_occurrences;
  bool has_arg;
  float num_value;
  char *string_value;
  t_linklist *strlist;
};

struct s_cmdline {
  int argc;
  char **argv;
  char *argv0;
  char **argv_string;
  char **argv_file;
  t_argsetup *argsetup;
  int count;
  int i;
  bool parsing_multiple;
  int parsing_mode;
  int overwrite_flags;
  /* t_opt *opts [CMDLINE_MAX_OPTS]; */
  t_linklist *opts;
  int errors;
  int unknown_args;
  int doubledash;
  
  int extra_args_max;
  /* int extra_args_count;
  char **extra_args; */
  t_linklist *extra_args;
};

#ifdef __cplusplus
class c_linklist;
extern "C" {
#endif

//#define _EXT extern
#define _EXT

/* cmdline_parse () returns a pointer to a cmdline structure, which must be freed afterwards
 * with cmdline_dealloc ()
 * 
 * Parameters:
 *   - argsetup: pointer to a t_argsetup structure, see cmdline.c for details
 *   - argc, argv: as passed to main ()
 *   - extra_args_max: maximum of arguments which do not begin with dash(es), ie. filenames
 * 
 * Pass NULL as argv to simply initialize a t_cmdline structure without reading any args.
 */
_EXT t_cmdline *cmdline_parse (t_argsetup *argsetup, int argc, char **argv, int extra_args_max);

/* cmdline_parse_string () parses an arbitrary string, returns number of args found.
 * If 'argsetup' is NULL, uses the same pointer that was passed to cmdline_parse ().
 * If 'overwrite' is false, ignores args that have already been read */
_EXT int cmdline_parse_string (t_cmdline *data, t_argsetup *argsetup, char *str, int overwrite_flags);

/* cmdline_parse_env () retrieves and parses an environment variable, and returns the
 * number of arguments found. */
_EXT int cmdline_parse_env (t_cmdline *data, t_argsetup *argsetup, char *envname, int overwrite_flags);

/* cmdline_parse_file () parses non-empty lines from a file that don't start with a
 * hash (pound) character, and returns the number of args found, or -1 if error.
 * The lines in the file do not have to start with a double dash. */
_EXT int cmdline_parse_file (t_cmdline *data, t_argsetup *argsetup, char *filename, int overwrite_flags);

/* Writes the options that are: marked known, AND specified, AND don't have 
 * errors, AND have the CMDLINE_WRITE_TO_CFG_FLAG set, to 'filename'. Useful for
 * quickly generating config files.
 * NOTE: searching for an option will mark it as known, so if it exists, it will be
 * written to the file, even if it's not in the argsetup table.
 * Returns number of args written, or -1 if error */
_EXT int cmdline_write_file (t_cmdline *data, char *filename, int overwrite_mode);

/* counts how many options have CMDLINE_WRITE_TO_CFG_FLAG set in its 'type' (see above) */
_EXT int cmdline_args_to_write_to_file (t_cmdline *data);

/* Deallocates the structure created by cmdline_parse () */
_EXT void cmdline_dealloc (t_cmdline *data);

/* These functions are used for retrieving command line data */
_EXT int    cmdline_option       (t_cmdline *data, char shortname, char *longname);
_EXT t_opt *cmdline_get_count    (t_cmdline *data, char shortname, char *longname, int *dest);
_EXT t_opt *cmdline_get_string   (t_cmdline *data, char shortname, char *longname, char *dest, int maxlen);
_EXT int    cmdline_get_strlist  (t_cmdline *data, char shortname, char *longname, t_linklist *dest);
_EXT t_opt *cmdline_get_bool     (t_cmdline *data, char shortname, char *longname, bool *dest);
_EXT t_opt *cmdline_get_int      (t_cmdline *data, char shortname, char *longname, int *dest);
_EXT t_opt *cmdline_get_longint  (t_cmdline *data, char shortname, char *longname, long int *dest);
_EXT t_opt *cmdline_get_float    (t_cmdline *data, char shortname, char *longname, float *dest);

/* These functions are used to set, add, or remove options, usually before writing to config file */
_EXT void   cmdline_remove       (t_cmdline *data, char shortnmae, char *longname);
_EXT t_opt *cmdline_set_count    (t_cmdline *data, char shortname, char *longname, int val);
_EXT t_opt *cmdline_set_string   (t_cmdline *data, char shortname, char *longname, char *val);
_EXT t_opt *cmdline_set_int      (t_cmdline *data, char shortname, char *longname, int val);
_EXT t_opt *cmdline_set_bool     (t_cmdline *data, char shortname, char *longname, bool val);
_EXT t_opt *cmdline_set_longint  (t_cmdline *data, char shortname, char *longname, long int val);
_EXT t_opt *cmdline_set_float    (t_cmdline *data, char shortname, char *longname, float val);

/* Does its best to print a coherent list of parsing errors */
_EXT int cmdline_print_errors (t_cmdline *data, FILE *output,
                                 bool include_unknown_args, bool include_extra_args);
                                 
/* Debug functions */
_EXT void cmdline_inspect (t_cmdline *data);
_EXT void cmdline_opt_inspect (t_opt *opt);


/* linklist stuff, might be useful elsewhere so i'm leaving it here */
_EXT t_linklist *linklist_init              (bool auto_unref_items);
_EXT t_linkitem *linklist_add_item          (t_linklist *list, void *data, size_t size);
_EXT t_linkitem *linklist_add_item_copy     (t_linklist *list, void *data, size_t size);
_EXT t_linkitem *linklist_insert_item       (t_linklist *list, t_linkitem *after, void *data, size_t size);
_EXT t_linkitem *linklist_insert_item_copy  (t_linklist *list, t_linkitem *after, void *data, size_t size);

_EXT bool linklist_remove_all               (t_linklist *list);
_EXT bool linklist_remove_null_items        (t_linklist *list);
_EXT bool linklist_remove_first             (t_linklist *list);
_EXT bool linklist_remove_last              (t_linklist *list);
_EXT bool linklist_remove_item              (t_linklist *list, t_linkitem *item);
_EXT bool linklist_remove_data              (t_linklist *list, void *ptr);

_EXT t_linkitem *linklist_get_next          (t_linklist *list);
_EXT t_linkitem *linklist_get_prev          (t_linklist *list);
_EXT t_linkitem *linklist_get_num           (t_linklist *list, int num);
_EXT t_linkitem *linklist_find_item         (t_linklist *list, void *ptr);

_EXT bool  linklist_from_array              (t_linklist *list, void *data, size_t itemsize, size_t datasize);
_EXT void *linklist_to_array                (t_linklist *list, size_t *r_itemsize, size_t *r_totalsize);

_EXT void linklist_sort                     (t_linklist *list, int (*func) (void *a, void *b), bool reverse);
_EXT void linklist_dealloc                  (t_linklist *list);
_EXT void linklist_inspect                  (t_linklist *list);


/* useful fancy debug function */

/* open/close the log file for debugging */
FILE *cmdline_open_debug_file (const char *path, const char *headertext);
void cmdline_close_debug_file ();
/* ...or just use these file handlers directly */
//extern FILE *g_debug_file;
//extern FILE *g_debug_out;

/* define your debug macro to this function, such as:
 * #define debug(...) cmdline_debug(stdout,NULL,__FILE__,__LINE__,__func__,__VA_ARGS__)
 * note, no space between func.name and (...)
 * NULL might be used for ansi color sequences to color prefix of each line */
int cmdline_debug (FILE *out, const char *color, const char *file,
                   int line, const char *func, const char *fmt, ...);

#ifdef __cplusplus
//#undef bool
}

/* C++ bindings / wrapper classes
 * TODO: template class for linklist */

class c_linklist {
public:
  inline c_linklist (bool auto_unref_items = false);
  inline ~c_linklist ();
  inline t_linkitem *add_item (void *data, size_t size);
  inline t_linkitem *add_item_copy (void *data, size_t size);
  inline t_linkitem *insert_item (t_linkitem *after, void *data, size_t size);
  inline t_linkitem *insert_item_copy (t_linkitem *after, void *data, size_t size);

  inline bool remove_all ();
  inline bool remove_null_items ();
  inline bool remove_first ();
  inline bool remove_last ();
  inline bool remove_item (t_linkitem *item);
  inline bool remove_data (void *ptr);
  
  inline t_linkitem *get_next ();
  inline t_linkitem *get_prev ();
  inline t_linkitem *get_num (int num);
  inline t_linkitem *find_item (void *ptr);
  
  inline int count ();
  inline t_linkitem *first ();
  inline t_linkitem *last ();
  inline t_linkitem *last_accessed ();
  
  inline bool  from_array (void *data, size_t itemsize, size_t datasize);
  inline void *to_array (size_t *r_itemsize, size_t *r_totalsize = NULL);
  
  inline bool is_sorted () { return list->sorted; }
  //void sort (int (*func) (void *a, void *b), bool reverse = false);
  inline void sort (cmpfunc_void *func, bool reverse = false);
  inline void inspect ();
  
//private:
  t_linklist *list;
};

class c_cmdline {
public:
  inline c_cmdline (t_argsetup *table, int argc, char **argv, int extra_max = 32767);
	inline c_cmdline (int argc, char **argv) : c_cmdline (NULL, argc, argv) { }
  inline ~c_cmdline ();
  inline int parse_string (t_argsetup *table, char *str, int overwrite_flags = CMDLINE_NO_OPTS);
  inline int parse_env (t_argsetup *table, char *envname, int overwrite_flags = CMDLINE_NO_OPTS);
  inline int parse_file (t_argsetup *table, char *file, int overwrite_flags = CMDLINE_NO_OPTS);
  inline int args_to_write_to_file ();
  inline int write_file (char *file, int overwrite_mode = CMDLINE_OVERWRITE);
  
  inline int argc ();
  inline char **argv ();

  inline int option (char s, char *l);
  inline t_linklist *opts ();
  inline t_linklist *extra_args ();
  inline t_opt *get_count (char s, char *l, int *dest = NULL);
  inline t_opt *get_string (char s, char *l, char *dest, int maxlen);
  inline int    get_strlist (char s, char *l, c_linklist *list);
  inline t_opt *get_bool (char s, char *l, bool *dest);
  inline t_opt *get_int (char s, char *l, int *dest);
  inline t_opt *get_longint (char s, char *l, long int *dest);
  inline t_opt *get_float (char s, char *l, float *dest);

  inline void   remove (char s, char *l);
  inline t_opt *set_count (char s, char *l, int val);
  inline t_opt *set_string (char s, char *l, char *val);
  inline t_opt *set_int (char s, char *l, int val);
  inline t_opt *set_bool (char s, char *l, bool val);
  inline t_opt *set_longint (char s, char *l, long int val);
  inline t_opt *set_float (char s, char *l, float val);

  inline int print_errors (FILE *output, bool include_unknown, bool include_extra);

  inline void inspect ();
  
private:
  t_cmdline *data;
};


/* boilerplate mess for C++ */

/* c_linklist class */

inline c_linklist::c_linklist (bool auto_unref_items)
  { list = linklist_init (auto_unref_items); }
inline c_linklist::~c_linklist ()
  { linklist_dealloc (list); }
  
inline t_linkitem *c_linklist::add_item (void *data, size_t size)
  { return linklist_add_item (list, data, size); }
inline t_linkitem *c_linklist::add_item_copy (void *data, size_t size)
  { return linklist_add_item_copy (list, data, size); }
inline t_linkitem *c_linklist::insert_item (t_linkitem *after, void *data, size_t size)
  { return linklist_insert_item (list, after, data, size); }
inline t_linkitem *c_linklist::insert_item_copy (t_linkitem *after, void *data, size_t size)
  { return linklist_insert_item_copy (list, after, data, size); }

inline bool c_linklist::remove_all ()
  { return linklist_remove_all (list); }
inline bool c_linklist::remove_null_items ()
  { return linklist_remove_null_items (list); }
inline bool c_linklist::remove_first ()
  { return linklist_remove_first (list); }
inline bool c_linklist::remove_last ()
  { return linklist_remove_last (list); }
inline bool c_linklist::remove_item (t_linkitem *item)
  { return linklist_remove_item (list, item); }
inline bool c_linklist::remove_data (void *ptr)
  { return linklist_remove_data (list, ptr); }
  
inline bool c_linklist::from_array (void *data, size_t itemsize, size_t datasize)
  { return linklist_from_array (list, data, itemsize, datasize); }
inline void *c_linklist::to_array (size_t *r_itemsize, size_t *r_totalsize)
  { return linklist_to_array (list, r_itemsize, r_totalsize); }
    
inline t_linkitem *c_linklist::get_next ()
  { return linklist_get_next (list); }
inline t_linkitem *c_linklist::get_prev ()
  { return linklist_get_prev (list); }
inline t_linkitem *c_linklist::get_num (int num)
  { return linklist_get_num (list, num); }
inline t_linkitem *c_linklist::find_item (void *ptr)
  { return linklist_find_item (list, ptr); }
  
inline int c_linklist::count ()
  { return list->count; }
inline t_linkitem *c_linklist::first ()
  { return list->first; }
inline t_linkitem *c_linklist::last ()
  { return list->last; }
inline t_linkitem *c_linklist::last_accessed ()
  { return list->last_accessed; }
  
inline void c_linklist::sort (int (*func) (void *a, void *b), bool reverse)
  { linklist_sort (list, func, reverse); }
  
inline void c_linklist::inspect ()
  { linklist_inspect (list); }


/* c_cmdline class */

inline c_cmdline::c_cmdline (t_argsetup *table, int argc, char **argv, int extra_max)
  { data = cmdline_parse (table, argc, argv, extra_max); }
inline c_cmdline::~c_cmdline ()
  { cmdline_dealloc (data); }
  
inline int c_cmdline::parse_string (t_argsetup *table, char *str, int overwrite_flags)
  { return cmdline_parse_string (data, table, str, overwrite_flags); }
inline int c_cmdline::parse_env (t_argsetup *table, char *envname, int overwrite_flags)
  { return cmdline_parse_env (data, table, envname, overwrite_flags); }
inline int c_cmdline::parse_file (t_argsetup *table, char *file, int overwrite_mode)
  { return cmdline_parse_file (data, table, file, overwrite_mode); }
inline int c_cmdline::write_file (char *file, int overwrite_mode)
  { return cmdline_write_file (data, file, overwrite_mode); }
inline int c_cmdline::args_to_write_to_file ()
  { return cmdline_args_to_write_to_file (data); }
  
inline int c_cmdline::argc ()
  { return data->argc; }
inline char **c_cmdline::argv ()
  { return data->argv; }
  
inline int c_cmdline::option (char s, char *l)
  { return cmdline_option (data, s, l); }
inline t_linklist *c_cmdline::opts ()
  { return data->opts; }
inline t_linklist *c_cmdline::extra_args ()
  { return data->extra_args; }
inline t_opt *c_cmdline::get_count (char s, char *l, int *dest)
  { return cmdline_get_count (data, s, l, dest); }
inline t_opt *c_cmdline::get_string (char s, char *l, char *dest, int maxlen)
  { return cmdline_get_string (data, s, l, dest, maxlen); }
inline int    c_cmdline::get_strlist (char s, char *l, c_linklist *dest) {
    return cmdline_get_strlist (data, s, l, dest->list); }
inline t_opt *c_cmdline::get_bool (char s, char *l, bool *dest)
  { return cmdline_get_bool (data, s, l, dest); }
inline t_opt *c_cmdline::get_int (char s, char *l, int *dest)
  { return cmdline_get_int (data, s, l, dest); }
inline t_opt *c_cmdline::get_longint (char s, char *l, long int *dest)
  { return cmdline_get_longint (data, s, l, dest); }
inline t_opt *c_cmdline::get_float (char s, char *l, float *dest)
  { return cmdline_get_float (data, s, l, dest); }
  
inline void c_cmdline::remove (char s, char *l)
  { cmdline_remove (data, s, l); }
inline t_opt *c_cmdline::set_count (char s, char *l, int val)
  { return cmdline_set_count (data, s, l, val); }
inline t_opt *c_cmdline::set_string (char s, char *l, char *val)
  { return cmdline_set_string (data, s, l, val); }
inline t_opt *c_cmdline::set_int (char s, char *l, int val)
  { return cmdline_set_int (data, s, l, val); }
inline t_opt *c_cmdline::set_bool (char s, char *l, bool val)
  { return cmdline_set_bool (data, s, l, val); }
inline t_opt *c_cmdline::set_longint (char s, char *l, long int val)
  { return cmdline_set_longint (data, s, l, val); }
inline t_opt *c_cmdline::set_float (char s, char *l, float val)
  { return cmdline_set_float (data, s, l, val); }
  
inline int c_cmdline::print_errors (FILE *output, bool include_unknown, bool include_extra)
  { return cmdline_print_errors (data, output, include_unknown, include_extra); }
  
inline void c_cmdline::inspect ()
  { cmdline_inspect (data); }

#else
//#undef bool
#endif

#ifdef DEBUG_CMDLINE
int linklist_test (int argc, char **argv);
int cmdline_test (int argc, char **argv);
#endif

#ifdef CMDLINE_ANSI

extern char   
CMDLINE_ANSI_GREY [],      CMDLINE_ANSI_BLACK [],
CMDLINE_ANSI_RED [],       CMDLINE_ANSI_DARK_RED [],
CMDLINE_ANSI_GREEN [],     CMDLINE_ANSI_DARK_GREEN [],
CMDLINE_ANSI_YELLOW [],    CMDLINE_ANSI_DARK_YELLOW [],
CMDLINE_ANSI_BLUE [],      CMDLINE_ANSI_DARK_BLUE [],
CMDLINE_ANSI_MAGENTA [],   CMDLINE_ANSI_DARK_MAGENTA [],
CMDLINE_ANSI_CYAN [],      CMDLINE_ANSI_DARK_CYAN [],
CMDLINE_ANSI_WHITE [],     CMDLINE_ANSI_DARK_GREY [],
CMDLINE_ANSI_RESET [];

#define ANSI_GREY          CMDLINE_ANSI_GREY
#define ANSI_RED           CMDLINE_ANSI_RED
#define ANSI_GREEN         CMDLINE_ANSI_GREEN
#define ANSI_YELLOW        CMDLINE_ANSI_YELLOW
#define ANSI_BLUE          CMDLINE_ANSI_BLUE
#define ANSI_MAGENTA       CMDLINE_ANSI_MAGENTA
#define ANSI_CYAN          CMDLINE_ANSI_CYAN
#define ANSI_WHITE         CMDLINE_ANSI_WHITE
#define ANSI_BLACK         CMDLINE_ANSI_BLACK
#define ANSI_DARK_RED      CMDLINE_ANSI_DARK_RED
#define ANSI_DARK_GREEN    CMDLINE_ANSI_DARK_GREEN
#define ANSI_DARK_YELLOW   CMDLINE_ANSI_DARK_YELLOW
#define ANSI_DARK_BLUE     CMDLINE_ANSI_DARK_BLUE
#define ANSI_DARK_MAGENTA  CMDLINE_ANSI_DARK_MAGENTA
#define ANSI_DARK_CYAN     CMDLINE_ANSI_DARK_CYAN
#define ANSI_DARK_GREY     CMDLINE_ANSI_DARK_GREY
#define ANSI_RESET         CMDLINE_ANSI_RESET

/* synonyms for some of the above */
#define ANSI_PURPLE        ANSI_MAGENTA
#define ANSI_DARK_PURPLE   ANSI_DARK_MAGENTA
#define ANSI_RED_DARK      ANSI_DARK_RED
#define ANSI_GREEN_DARK    ANSI_DARK_GREEN
#define ANSI_YELLOW_DARK   ANSI_DARK_YELLOW
#define ANSI_BLUE_DARK     ANSI_DARK_BLUE
#define ANSI_MAGENTA_DARK  ANSI_DARK_MAGENTA
#define ANSI_PURPLE_DARK   ANSI_DARK_PURPLE
#define ANSI_CYAN_DARK     ANSI_DARK_CYAN

#endif

#undef __IN_CMDLINE_H
#endif



/****************************************************************************
 * IMPLEMENTATION FOLLOWS!!
 *
 * If this is included from a .c or .cpp file (not a header file) then just
 * #define CMDLINE_IMPLEMENTATION
 * to expand this. This approach is easier than having a separate library to
 * build and link against...
 *
 ****************************************************************************/


#ifdef CMDLINE_IMPLEMENTATION


/* changed debug(...) macro to this so it won't interfere with the rest of
   file this is included from, which might use "debug" as name for something */
#define _cmdline_debug(...)


/* 
 * Take care of these command line shenanigans
 * ONCE AND FOR ALL.
 * Without wasting "too much" memory and/or speed.
 * 
 *   - Éric. (delt)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>

//#include "cmdline.h" ....duh

#ifdef C89
#define CMDLINE_PATH_MAX 256
#else
#define CMDLINE_PATH_MAX 4096
#endif

//#define DEBUG_CMDLINE
static const char *debug_color = "\x1B[1;30m";

// debugging stuff
/*
#ifdef BP
# undef BP
#endif
#ifdef DEBUG_CMDLINE_LINKLIST
#define debug(...) _internal_debug(debug_color,__FILE__,__LINE__,__func__,__VA_ARGS__)
#warning "BREAKPOINT MACRO DEFINED"
#define BP(...) {printf ("\n\x1B[1m____BREAKPOINT____\x1B[0;37m\n");debug(__VA_ARGS__);fflush(stdout);getc(stdin);}
#else
// TODO: something for really old compilers that don't support varidaic macros
#define debug(...)
#define BP(...)
#endif
*/

#ifndef __cplusplus
#define bool char
#endif

/* Example "argsetup" table. See below for more information. Basically, you'd
 * have a static instance of this which represents the different command-line
 * options that your program can accept, specifying if each is a string, number,
 * count (how many times it occurs, like -v in many Unix utils) and so on.
 * Once you've created a c_cmdline instance (C++) or a t_cmdline (plain C)
 * with your argsetup structure, then you can retrieve option values with the
 * cmdline_get_* functions (again, see header file) depending on type of data
 * expected, and so on. */
 
#ifdef _EXAMPLE_ARGSETUP_TABLE

static t_argsetup example_argsetup [] = {
  /*  short  long               type         min     max */
  {   'i',   "--test-int",      CMD_INT,     0,      5        },
  {   's',   "--test-string",   CMD_STRING,  0,      0        },
  {   'S',   "--test-strlist",  CMD_STRING,  0,      0        },
  {   'f',   "--test-float",    CMD_FLOAT,   0,      4.5      },
  {   'b',   "--test-bool",     CMD_BOOL,    0,      1        },
  {   'd',   "--duplicate",     CMD_BOOL,    0,      1        },
  {   'd',   "--duplicate",     CMD_BOOL,    0,      1        },
  {   'c',   "--test-count",    CMD_COUNT,   0,      8        },
  {   ' ',   "--no-short-name", CMD_VOID,    0,      0        },
  {   'n',   "",                CMD_VOID,    0,      0        },
  {   'N',   NULL,              CMD_VOID,    0,      0        },
  {   0,     NULL,              CMD_NONE,    0,      0        }
};

#endif


char CMDLINE_ANSI_BLACK [] =          "\x1B[0;30m";
char CMDLINE_ANSI_DARK_RED [] =       "\x1B[0;31m";
char CMDLINE_ANSI_DARK_GREEN [] =     "\x1B[0;32m";
char CMDLINE_ANSI_DARK_YELLOW [] =    "\x1B[0;33m";
char CMDLINE_ANSI_DARK_BLUE [] =      "\x1B[0;34m";
char CMDLINE_ANSI_DARK_MAGENTA [] =   "\x1B[0;35m";
char CMDLINE_ANSI_DARK_CYAN [] =      "\x1B[0;36m";
char CMDLINE_ANSI_DARK_GREY [] =      "\x1B[1;30m";
char CMDLINE_ANSI_GREY [] =           "\x1B[1;30m";
char CMDLINE_ANSI_RED [] =            "\x1B[1;31m";
char CMDLINE_ANSI_GREEN [] =          "\x1B[1;32m";
char CMDLINE_ANSI_YELLOW [] =         "\x1B[1;33m";
char CMDLINE_ANSI_BLUE [] =           "\x1B[1;34m";
char CMDLINE_ANSI_MAGENTA [] =        "\x1B[1;35m";
char CMDLINE_ANSI_CYAN [] =           "\x1B[1;36m";
char CMDLINE_ANSI_WHITE [] =          "\x1B[1;37m";
char CMDLINE_ANSI_RESET [] =          "\x1B[0m";


/* forward declares */
static t_opt *cmdline_find_opt (t_cmdline *data, char shortname, char *longname);
static t_linkitem *cmdline_find_opt_linkitem (t_cmdline *data, char shortname, char *longname);
static t_opt *cmdline_add_opt (t_cmdline *data, char shortname, char *longname);
static t_opt *cmdline_find_or_add_opt (t_cmdline *data, char shortname, char *longname);
static void *cmdline_malloc (size_t size);
static bool cmdline_is_number (char *s);
static bool cmdline_is_hex (char *s);
static bool is_comment_or_empty (char *s);
//static int cmdline_strcmp (char *str, char *opt, long int *eqreturn);
static int cmdline_strcmp (const char *str, const char *opt, long int *eqreturn);
static bool opt_match (t_opt *opt, char shortname, char *longname);
static bool cmdline_check_minmax (float value, float min, float max);
static bool add_extra_arg (t_cmdline *data, char *name);
static bool file_write_one_entry (FILE *f, t_opt *opt);
static int line_length (char *str);
static void cmdline_add_to_strlist (t_opt *opt, char *str, int len);

// *static* / FILE *g_debug_file = stdout;
// *static* / FILE *g_debug_out = stdout;


/*
FILE *cmdline_open_debug_file (const char *path, const char *headertext) {
  g_debug_file = fopen (path, "w");
  if (g_debug_file && headertext) {
    fprintf (g_debug_file, "%s", headertext);
  }
  return g_debug_file;
}


void cmdline_close_debug_file () {
  fclose (g_debug_file);
  g_debug_file = NULL;
}
*/

/* Fancy color debug function.
 * Adds the identifier string "file:linenumber:function" to the start of each non-empty line.
 * Strips param. list and return type / qualifiers from function name, leaving class:: if any.
 * Also adds a final newline if not present.
 * NOTE: Heavily using this function adss LOTS of overhead!!!! */
//#define BIGFMT_MAX 8192
int cmdline_debug (FILE *out, const char *color, const char *file, int line,
                   const char *func, const char *fmt, ...) {
  char funcname [256],
       c, *c1, *c2,
       *scan,
       *func_only,
       *bigfmt,//bigfmt [BIGFMT_MAX + 16],
       *filebase = (char *) file,
       idstring [512];
        
  va_list v;
  int i, j, retval, idlen, totallen;
  bool add_newline = FALSE;
  t_linklist *list;
  t_linkitem *item;
  
// TODO: come up with a better name for this macro
#define FANCY_DEBUG
#ifdef FANCY_DEBUG
  if (!color)
    color = /*"\033[1;30m"*/ CMDLINE_ANSI_DARK_GREY;
#endif
  
  strncpy (funcname, (char *) func, 255);
  
  /* strip off param list */
  c1 = strchr (funcname, '(');
  c2 = funcname;
  if (c1)
    *c1 = 0;
  
  /* strip off return type and qualifiers */
  do {
    c1 = strchr (c2, ' ');
    if (c1)
      c2 = c1 + 1;
  } while (c1);
  func_only = c2;
  
#ifndef DEBUG_FULL_PATHS
  /* strip off beginning of filename */
  for (i = 0; file [i]; i++)
    if (file [i] == '/')
      filebase = ((char *) file) + i + 1;
#endif
    
  snprintf (idstring,
            127,
            "%s[%s:%d %s] %s",
            color ? color : "",
            filebase,
            line,
            func_only,
            color ? CMDLINE_ANSI_RESET /*"\x1B[0;37m"*/ : "");
  
  idlen = strlen (idstring);
  //printf ("idstring='%s', idlen=%d\n", idstring, idlen);
  
  /* expand fmt to contain the identifier string at the start of each non-empty line */
  /* try #2, using dynamic buffers / linklist */
  totallen = 0;
  list = linklist_init (TRUE);
  scan = (char *) fmt;
  while (*scan) {
    i = line_length (scan);
    linklist_add_item_copy (list, scan, i);
    scan += i;
    totallen += i + 3;
    if (i > 0)
      totallen += idlen;
    //printf ("*scan=%d\n", (int) *scan);
    if (*scan)
      scan++;
  }
  
  /*printf ("totallen=%d, list->count=%d\n", totallen, list->count);
  linklist_inspect (list);*/
  
  bigfmt = (char *) cmdline_malloc (totallen + 2);
  memset (bigfmt, 0, totallen);
  bigfmt [0] = 0;
  scan = bigfmt;
  item = list->first;
  while (item) {
    if (item->size > 0) {
      strncat (scan, idstring, idlen);
      scan += idlen;
      strncat (scan, (char *) item->data, item->size);
      scan += item->size;
    }
    *scan = '\n';
    if (*scan)
      scan++;
    item = item->next;
  }
  *scan = 0;
  
  /*printf ("add_newline=%d\n", add_newline);
  printf ("\nbigfmt looks like: '%s'\n", bigfmt);*/
  
  if (1/*__global_debug*/) {
    
		va_start (v, fmt);
		vfprintf (out, bigfmt, v);
		va_end (v);
#if 0
    va_start (v, fmt);
#ifdef WIN32ASDF
    retval = vprintf (bigfmt, v);
#else
    //if (!g_debug_out)
    //  g_debug_out = stderr;
    retval = vfprintf (out, bigfmt, v);
#endif
    va_end (v);
#endif
    /*if (add_newline) {
      printf ("\n");
    }*/
  }
  
  free (bigfmt);
  linklist_dealloc (list);
  return retval;
}


int line_length (char *str) {
  int retval = 0;
  while (*str && *str != 0x0A && *str != 0x0D) {
    retval++;
    str++;
  }
  
  return retval;
}


/* for debugging linklist stuff which uses debug macro TODO: fix this */
static int _internal_debug (const char *color, const char *file, int line, const char *func, const char *fmt, ...) {
  va_list v;
  int retval;
	FILE *g_debug_file = stdout;
  
  if (g_debug_file) {
    fprintf (g_debug_file, "%s%s:%d %s: \x1B[0;37m", color, file, line, func);
    
    va_start (v, fmt);
    vfprintf (g_debug_file, fmt, v);
    va_end (v);
  }
  
  fprintf (stderr, "[%s%s:%d %s] \x1B[0;37m", color, file, line, func);
  
  va_start (v, fmt);
  retval = vfprintf (stderr, fmt, v);
  va_end (v);
  
  return retval;
}


/* These functions are used to retrieve commandline data. The first argument
 * is a pointer to the t_cmdline structure created by cmdline_parse () followed
 * by short option name (character) and long option name (string) */


/* chooses type and returns num value of specified option. Meant for quickly
 * parsing an option without needing to pass an rpointer for the return value */
int cmdline_option (t_cmdline *data, char shortname, char *longname) {
  t_opt *opt = cmdline_find_opt (data, shortname, longname);
  int retval = 0;
  
  if (opt) {
    //opt->unknown = FALSE;
    opt->lookup_count++;
    switch (opt->type) {
      case CMD_COUNT:
      case CMD_INT:
      case CMD_BOOL:
      case CMD_VOID:
        retval = opt->num_value;
      break;
      
      default:
        retval = opt->num_occurrences;
      break;
    }
    _cmdline_debug ("%c(%s) not found, returning %d\n",
           shortname, longname, retval);
    return retval;
  } else {
    _cmdline_debug ("%c(%s) not found, returning 0\n",
           shortname, longname);
    return 0;
  }
}


/* similar to cmdline_option () but returns a pointer to the t_opt struct. */
t_opt *cmdline_get_count (t_cmdline *data, char shortname, char *longname, int *dest) {
  t_opt *opt = cmdline_find_opt (data, shortname, longname);

  /*if (opt && dest && cmdline_check_minmax (opt->num_occurrences, opt->min, opt->max)) {
    *dest = opt->num_occurrences;
  }*/
  
  if (!opt)
    return NULL;
  
  opt->lookup_count++;
  //opt->unknown = FALSE;
  
  if (dest)
    *dest = opt->num_occurrences;
  
  if (opt->min < opt->max) {
    if (*dest > opt->max) {
      *dest = opt->max;
    }
    if (*dest < opt->min) {
      *dest = opt->min;
    }
  }
  return opt;
}


/* Tries to find a string value, and returns it (up to 'maxlen' characters) */
t_opt *cmdline_get_string (t_cmdline *data, char shortname, char *longname,
                           char *dest, int maxlen) {  
  t_opt *opt = cmdline_find_opt (data, shortname, longname);
  
  _cmdline_debug ("start, shortname=%c, longname=%s\n", shortname, longname);
  
  if (!opt) {
    _cmdline_debug ("opt not found\n");
    return NULL;
  }
  
  opt->lookup_count++;
  //opt->unknown = FALSE;
  
  if (maxlen > opt->max && opt->max > opt->min) {
    if (opt->unknown) {
      maxlen = strlen (opt->string_value);
    } else {
      maxlen = opt->max;
    }
  }
  
  _cmdline_debug ("shortname=%c, longname=%s, maxlen=%d\n",
         shortname, longname, maxlen);
  if (opt->string_value) {
    _cmdline_debug ("\n\nopt=%d, arg string value=%s", opt->index, opt->string_value);
    strncpy (dest, opt->string_value, maxlen);
    _cmdline_debug ("returning t_opt pointer\n");
    return opt;
  }
  _cmdline_debug ("end, returning NULL\n");
  return NULL;
}

/* manage/return a list of strings for options which come up multiple
   times, like for example, -f file1 -f file2 -f file3 */
int cmdline_get_strlist (t_cmdline *data, char shortname, char *longname, t_linklist *dest) {
  int retval = 0;
  _cmdline_debug ("shortname='%c', longname=%s\n", shortname, longname);
  
  t_opt *opt = cmdline_find_opt (data, shortname, longname);
  if (opt && opt->strlist) {
    t_linkitem *item = opt->strlist->first;
    while (item) {
      linklist_add_item_copy (dest, item->data, strlen ((char *) item->data) + 2);
      retval++;
      item = item->next;
    }
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}

/* these next ones should be pretty self-explanatory */

t_opt *cmdline_get_float (t_cmdline *data, char shortname, char *longname, float *dest) {
  float value;
  t_opt *opt = cmdline_find_opt (data, shortname, longname);
  
  _cmdline_debug ("start, shortname=%c, longname=%s\n", shortname, longname);
  
  if (opt) {
    opt->lookup_count++;
    //opt->unknown = FALSE;
    _cmdline_debug ("opt->index=%d, arg num value=%f", opt->index, opt->num_value);
    value = opt->num_value;
    if (cmdline_check_minmax ((float) value, opt->min, opt->max)) {
      if (dest)
        *dest = value;
      _cmdline_debug ("return opt\n");
      return opt;
    } else {
      _cmdline_debug ("return NULL (1)\n");
      return NULL;
    }
  }
  _cmdline_debug ("return NULL (2)\n");
  return NULL;
}


t_opt *cmdline_get_int (t_cmdline *data, char shortname, char *longname, int *dest) {
  float temp;
  t_opt *opt;

  _cmdline_debug ("start, shortname=%c, longname='%s'\n", shortname, longname);
  
  opt = cmdline_get_float (data, shortname, longname, &temp);
  
  if (opt && dest)
    *dest = (int) temp;
  
  _cmdline_debug ("end\n");
  return opt;
}


t_opt *cmdline_get_longint (t_cmdline *data, char shortname, char *longname, long int *dest) {
  float temp;
  t_opt *opt;
  
  _cmdline_debug ("start, shortname=%c, longname='%s'\n", shortname, longname);
  
  opt = cmdline_get_float (data, shortname, longname, &temp);
  
  if (opt && dest)
    *dest = (int) temp;
  
  _cmdline_debug ("end\n");
  return opt;
}


t_opt *cmdline_get_bool (t_cmdline *data, char shortname, char *longname, bool *dest) {
  int intdest;
  t_opt *retval = cmdline_get_int (data, shortname, longname, &intdest);
  
  if (retval && dest) {
    if (intdest)
      *dest = TRUE;
    else
      *dest = FALSE;
  }
  
  //return TRUE; /* uh wut? */
  return retval;
}


static void cmdline_add_to_strlist (t_opt *opt, char *str, int len) {
  char buf [64];
  
  _cmdline_debug ("start");
  
  if (!opt->strlist) {
    opt->strlist = linklist_init (TRUE);
  }
  linklist_add_item_copy (opt->strlist, str, len);
  
  /* set num and string values to count in list */
  opt->num_value = opt->strlist->count;
  if (opt->string_value)
    free (opt->string_value);
  sprintf (buf, "%ld", (long int) opt->strlist->count);
  _cmdline_debug ("allocating memory\n");
  opt->string_value = (char *) cmdline_malloc ((size_t) strlen (buf) + 4);
  strcpy (opt->string_value, buf);
      
  _cmdline_debug ("end");
}


static t_opt *cmdline_set_option (t_cmdline *data, char shortname, char *longname,
                                  char *string_value, float num_value, int type) {
  int i, len;
  t_opt *opt;
  
  _cmdline_debug ("start: shortname=%c, longname=%s, string_value=%s, num_value=%f, type=%d\n",
         shortname, longname, string_value, num_value, type);
  
  opt = cmdline_find_or_add_opt (data, shortname, longname);
  opt->unknown = FALSE;
  
  //opt->write_to_file = (type & CMDLINE_WRITE_TO_CFG_FLAG) ? TRUE : FALSE;
  // don't set to zero if not present, will overwrite values from set_* funcs
  if (type & CMDLINE_WRITE_TO_CFG_FLAG)
    opt->write_to_file = TRUE;
  type &= 0xFF;//(~CMDLINE_WRITE_TO_CFG_FLAG);
  
  opt->type = type;
  
  if (shortname != ' ' && shortname != 0)
    opt->shortname = shortname;
  
  if (longname)
    strncpy (opt->longname, longname, CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH - 1);
  
  if (opt->type == CMD_STRING) {
    if (string_value) {
      if (opt->string_value)
        free (opt->string_value);
      len = strlen (string_value);
      if (len > CMDLINE_MAX_LENGTH)
        len = CMDLINE_MAX_LENGTH;
      _cmdline_debug ("allocating memory\n");
      opt->string_value = (char *) cmdline_malloc ((size_t) len + 4);
      strcpy (opt->string_value, string_value);
    }
  } else if (opt->type == CMD_STRLIST) {
    if (string_value) {
      cmdline_add_to_strlist (opt, string_value, len + 4);
    }
  } else {
    opt->num_value = num_value;
  }
  
  _cmdline_debug ("end, returning opt=0x%lX\n", (long int) opt);
  return opt;
}

t_opt *cmdline_set_string   (t_cmdline *data, char shortname, char *longname, char *val) {
  _cmdline_debug ("start, shortname=%c, longname=%s, val=%s\n",
         shortname, longname, val);
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, val, 0, CMD_STRING);
}

t_opt *cmdline_set_float    (t_cmdline *data, char shortname, char *longname, float val) {
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, NULL, val, CMD_FLOAT);
}

t_opt *cmdline_set_count    (t_cmdline *data, char shortname, char *longname, int val) {
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, NULL, (float) val, CMD_COUNT);
}

t_opt *cmdline_set_int      (t_cmdline *data, char shortname, char *longname, int val) {
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, NULL, (float) val, CMD_INT);
}

t_opt *cmdline_set_longint  (t_cmdline *data, char shortname, char *longname, long int val) {
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, NULL, (float) val, CMD_LONGINT);
}

t_opt *cmdline_set_bool     (t_cmdline *data, char shortname, char *longname, bool val) {
  _cmdline_debug ("calling cmdline_set_option\n");
  return cmdline_set_option (data, shortname, longname, NULL, (float) val, CMD_BOOL);
}




/* misc. memory management stuff, link list etc. */


t_linklist *linklist_init (bool auto_unref_items) {
  t_linklist *list;
  
  //_cmdline_debug ("start\n");
  
  //_cmdline_debug ("allocating memory\n");
  list = (t_linklist *) cmdline_malloc (sizeof (t_linklist));
  list->count = 0;
  list->auto_unref_items = auto_unref_items;
  list->sorted = TRUE;
  list->first = NULL;
  list->last = NULL;
  list->last_accessed = NULL;
  
  list->malloc_func = cmdline_malloc;
  list->free_func = free;
  
  //_cmdline_debug ("end\n");
  return list;
}


t_linkitem *linklist_add_item_backend (t_linklist *list, void *data,
                                       size_t size, bool make_copy) {
  t_linkitem *last;
  int retval = TRUE;
  
  _cmdline_debug ("start, data=0x%lX\n", (long int) data);
  
  if (size == -1)
    size = strlen ((char *) data);
  
  if (!list->count) { /* first item */
    _cmdline_debug ("(1) allocating memory\n");
    list->first = (t_linkitem *) cmdline_malloc (sizeof (t_linkitem)/* + size + 4*/);
    list->first->prev = NULL;
    list->last = list->first;
    last = list->first;
  } else {
    last = list->last;
    while (last->next) { /* shouldn't happen */
      last = last->next;
    }
    //last = list->last;
    _cmdline_debug ("(2) allocating memory\n");
    last->next = (t_linkitem *) cmdline_malloc (sizeof (t_linkitem)/* + size + 4*/);
    last = last->next;
    last->prev = list->last;
  }
  _cmdline_debug ("checkpoint 1\n");
  last->next = NULL;
  last->index = list->count;
  list->count++;
  last->size = size;
  if (make_copy) {
    last->is_copy = TRUE;
    //last->data = (void *) cmdline_malloc ((size_t) size);
    _cmdline_debug ("(3) allocating memory\n");
    last->data = (void *) (*list->malloc_func) ((size_t) size);
    memcpy (last->data, data, (size_t) size);
  } else {
    _cmdline_debug ("not making copy\n");
    last->is_copy = FALSE;
    last->data = data;
  }
  //last->data = last + sizeof (t_linkitem) + 2;
  list->last = last;
  list->sorted = FALSE;
  
  _cmdline_debug ("end\n");
  return last;
}


t_linkitem *linklist_add_item (t_linklist *list, void *data, size_t size) {
  return linklist_add_item_backend (list, data, size, FALSE);
}


t_linkitem *linklist_add_item_copy (t_linklist *list, void *data, size_t size) {
  return linklist_add_item_backend (list, data, size, TRUE);
}


t_linkitem *linklist_insert_item_backend (t_linklist *list, t_linkitem *after,
                                          void *data, size_t size, bool make_copy) {
  
  t_linkitem *newitem = (t_linkitem *) cmdline_malloc (sizeof (t_linkitem)/* + size + 4*/);
  
  if (size == -1)
    size = strlen ((char *) data);
  
  if (make_copy) {
    newitem->is_copy = TRUE;
    //newitem->data = (void *) cmdline_malloc ((size_t) size);
    newitem->data = (void *) (*list->malloc_func) ((size_t) size);
    memcpy (newitem->data, data, (size_t) size);
  } else {
    newitem->is_copy = FALSE;
    newitem->data = data;
  }
  newitem->size = size;
  list->sorted = FALSE;
  
  if (!list->count || after == NULL) {
    /* add to end of list */
    return linklist_add_item_backend (list, data, size, make_copy);
  } else {
    /* insert before 'item' */
    t_linkitem *next = after->next;
    if (next) {
      next->prev = newitem;
      after->next = newitem;
      newitem->prev = after;
      newitem->next = next;
    }
  }
  list->count++;
  return newitem;
}


t_linkitem *linklist_insert_item (t_linklist *list, t_linkitem *after, void *data, size_t size) {
  return linklist_insert_item_backend (list, after, data, size, FALSE);
}


t_linkitem *linklist_insert_item_copy (t_linklist *list, t_linkitem *after, void *data, size_t size) {
  return linklist_insert_item_backend (list, after, data, size, TRUE);
}


bool linklist_remove_all (t_linklist *list) {
  
  _cmdline_debug ("start\n");
  
  list->sorted = TRUE;
  
  if (!list->first) {
    _cmdline_debug ("list already empty\n");
    return FALSE;
  }
  
  while (list->first) {
    linklist_remove_first (list);
  }
  
  _cmdline_debug ("end\n");
  return TRUE;
}


bool linklist_remove_null_items (t_linklist *list) {
  bool retval = FALSE;
  t_linkitem *item, *nextitem;
  _cmdline_debug ("start");
  
  item = list->first;
  while (item) {
    nextitem = item->next;
    if (!item->data) {
      _cmdline_debug ("removing null entry at 0x%lX", (long int) item);
      linklist_remove_item (list, item);
      retval = TRUE;
    }
    item = nextitem;
  }
  
  _cmdline_debug ("end");
  return retval;
}


bool linklist_remove_first (t_linklist *list) {
  t_linkitem *first = list->first;
  _cmdline_debug ("start\n");
  //_cmdline_debug ("start, last=0x%lX\n", (long int) list->last);
  
  if (first) {
    list->first = list->first->next;
    if (list->first)
      list->first->prev = NULL;
    if ((first->is_copy || list->auto_unref_items) && first->data) {
      _cmdline_debug ("freeing data pointer at 0x%lX\n", (long int) first->data);
      //free (first->data);
      (*list->free_func) (first->data);
      first->data = NULL;
      _cmdline_debug ("done\n");
    }
    _cmdline_debug ("freeing entry pointer at 0x%lX\n", (long int) first);
    free (first);
      _cmdline_debug ("done\n");
    list->count--;
    if (!list->first)
      list->last = NULL;
    //_cmdline_debug ("returning TRUE\n");
    return TRUE;
  } else {
    //_cmdline_debug ("returning FALSE\n");
    return FALSE;
  }
}


bool linklist_remove_last (t_linklist *list) {
  t_linkitem *last = list->last;

  //_cmdline_debug ("start, last=0x%lX\n", (long int) list->last);

  if (last) {
    list->last = list->last->prev;
    if (list->last)
      list->last->next = NULL;
    if ((last->is_copy || list->auto_unref_items) && last->data) {
      //free (last->data);
      (*list->free_func) (last->data);
      last->data = NULL;
    }
    free (last); /* SEGFAULT HERE */
    list->count--;
    if (!list->last)
      list->first = NULL;
    //_cmdline_debug ("returning TRUE\n");
    return TRUE;
  } else {
    //_cmdline_debug ("returning FALSE\n");
    return FALSE;
  }
}


bool linklist_remove_item (t_linklist *list, t_linkitem *item) {
  if (item == list->first) {
    linklist_remove_first (list);
  } else if (item == list->last) {
    //linklist_inspect (list);
    linklist_remove_last (list);
  } else {
    if (item->is_copy || list->auto_unref_items)
      //free (item->data);
      (*list->free_func) (item->data);
    list->count--;
    if (item->next)
      item->next->prev = item->prev;
    if (item->prev)
      item->prev->next = item->next;
  }
  return TRUE; /* no reason this would return false */
}


bool linklist_remove_data (t_linklist *list, void *ptr) {
  t_linkitem *item;
  
  item = linklist_find_item (list, ptr);
  if (item) {
    return linklist_remove_item (list, item);
  } else {
    return FALSE;
  }
}


t_linkitem *linklist_get_next (t_linklist *list) {

  //_cmdline_debug ("start\n");
  
  if (!list->last_accessed) {
    list->last_accessed = list->first;
    //_cmdline_debug ("return list->first\n");
    return list->first;
  } else {
    list->last_accessed = list->last_accessed->next;
    //_cmdline_debug ("return list->last_accessed\n");
    return list->last_accessed;
  }
}


t_linkitem *linklist_get_prev (t_linklist *list) {
  _cmdline_debug ("start\n");
  
  if (!list->last_accessed) {
    list->last_accessed = list->last;
    _cmdline_debug ("return list->last\n");
    return list->last;
  } else {
    list->last_accessed = list->last_accessed->prev;
    _cmdline_debug ("return list->last_accessed\n");
    return list->last_accessed;
  }
}


t_linkitem *linklist_get_num (t_linklist *list, int num) {
  int i = 0;
  t_linkitem *retval;
  
  if (!num) {
    list->last_accessed = list->first;
    return list->first;
  } else {
    retval = list->first;
    while (i < num) {
      retval = retval->next;
      if (!retval)
        return NULL;
      i++;
    }
    list->last_accessed = retval;
    return retval;
  }
}


t_linkitem *linklist_find_item (t_linklist *list, void *data) {
  t_linkitem *retval = list->first;
  
  while (retval && retval->data != data) {
    retval = retval->next;
  }
  
  return retval;
}

#define USE_QSORT

/* dirty macro hack, but we need this
   TODO: test if we can do this faster, ie. memcpy () or own loop/func
         (....probably not)
   We need to swap:
     the data pointer
     the size,
     the is_copy flag
     userdata0, userdata1
*/
#define EXCH(a,b) {                      \
          swap_p_item = a->data;         \
          a->data = b->data;             \
          b->data = swap_p_item;         \
          swap_size = a->size;           \
          a->size = b->size;             \
          b->size = swap_size;           \
          swap_b = a->is_copy;           \
          a->is_copy = b->is_copy;       \
          b->is_copy = swap_b;           \
          swap_userdata = a->userdata0;  \
          a->userdata0 = b->userdata0;   \
          b->userdata0 = a->userdata0;   \
          swap_userdata = a->userdata1;  \
          a->userdata1 = b->userdata1;   \
          b->userdata1 = a->userdata1;   \
          }

#ifdef USE_QSORT

/* more dirty macro hacks */
#define EQ(a,b) ((*cmpfunc)(a->data,b->data)==0)
#define GT(a,b) ((*cmpfunc)(a->data,b->data)>0)
#define LT(a,b) ((*cmpfunc)(a->data,b->data)<0)

static void linklist_qsort_recurse (t_linkitem **a, int l, int r, 
                                    int (*cmpfunc) (void *d1, void *d2)) {
  void *swap_p_item = NULL;
  uint64_t swap_userdata;
  long int swap_size = 0;
  bool swap_b = FALSE;
  
  //_cmdline_debug ("linklist_qsort_recurse: start, l=%d, r=%d\n", l, r);
  
  int i = l - 1, j = r, p = l - 1, q = r, k = 0;
  t_linkitem *v = a [r];
  if (r <= l)
    return;
  for (;;) {
    while (LT (a [++i], v));      // while (a [++i] < v);
    while (LT (v, a [--j]))       // while (v < a [--j])
      if (j == l)
        break;
    if (i >= j)
      break;
  EXCH (a [i], a [j]);
    if (EQ (a [i], v)) {          //if (a [i] == v) {
      p++;
      EXCH (a [p], a [i]);
    }
    if (EQ (v, a [j])) {          //if (v == a [j]) {
      q--;
      EXCH (a [j], a [q]);
    }
  }
  EXCH (a [i], a [r]);
  j = i - 1;
  i = i + 1;
  for (k = l; k < p; k++, j--) {
    EXCH (a [k], a [j]);
  }
  for (k = r - 1; k > q; k--, i++) {
    EXCH (a [i], a [k]);
  }
  linklist_qsort_recurse (a, l, j, cmpfunc);
  linklist_qsort_recurse (a, i, r, cmpfunc);
  
  //_cmdline_debug ("linklist_qsort_recurse: end\n", l, r);
}


/* either almost exact duplicate code or lots of unnecessary parameters.... */
static void linklist_qsort_recurse_reverse (t_linkitem **a, int l, int r, 
                                    int (*cmpfunc) (void *d1, void *d2)) {
  void *swap_p_item = NULL;
  long int swap_size = 0;
  bool swap_b = FALSE;
  uint64_t swap_userdata;
  
  //_cmdline_debug ("linklist_qsort_recurse_reverse: start, l=%d, r=%d\n", l, r);
  
  int i = l - 1, j = r, p = l - 1, q = r, k = 0;
  t_linkitem *v = a [r];
  if (r <= l)
    return;
  for (;;) {
    while (GT (a [++i], v));      // while (a [++i] < v);
    
    while (GT (v, a [--j]))       // while (v < a [--j])
      if (j == l)
        break;
    if (i >= j)
      break;
    EXCH (a [i], a [j]);
    if (EQ (a [i], v)) {          //if (a [i] == v) {
      p++;
      EXCH (a [p], a [i]);
    }
    if (EQ (v, a [j])) {          //if (v == a [j]) {
      q--;
      EXCH (a [j], a [q]);
    }
  }
  EXCH (a [i], a [r]);
  j = i - 1;
  i = i + 1;
  for (k = l; k < p; k++, j--) {
    EXCH (a [k], a [j]);
  }
  for (k = r - 1; k > q; k--, i++) {
    EXCH (a [i], a [k]);
  }
  linklist_qsort_recurse_reverse (a, l, j, cmpfunc);
  linklist_qsort_recurse_reverse (a, i, r, cmpfunc);
  
  //_cmdline_debug ("linklist_qsort_recurse_reverse: end\n", l, r);
}


#undef EQ
#undef GT
#undef LT

#endif

void linklist_sort (t_linklist *list, cmpfunc_void *cmpfunc, bool reverse) {
  bool valid_list = TRUE;
  int i, j, k;
  t_linkitem *item;
  t_linkitem **p_items;
  /*void *swap_p_item;
  long int swap_size;*/
  bool swap_b;
  
  //_cmdline_debug ("start\n");
  
  //linklist_inspect (list);
  
  /* round up a flat array of pointers to all t_linkitem structs */
  p_items = (t_linkitem **) cmdline_malloc (list->count * sizeof (void ***));
  i = 0;
  item = list->first;
  while (item && i < list->count) {
    p_items [i] = item;
    i++;
    item = item->next;
  }
  
  /* check if count is correct */
  if (i != list->count) {
    printf ("ERROR: linklist corruption: number of items less than list count!\n");
    valid_list = FALSE;
  }
  if (item) {
    printf ("ERROR: linklist corruption: number of items greater than list count!\n");
    valid_list = FALSE;
  }
  
  /* sort pointers using cmpfunc */
  if (valid_list) {
#ifndef USE_QSORT
    /* use a simple bubble sort for now... */
    /* DONE: implement a qsort algorithm to replace this, see just below */
    for (i = 0; i < list->count; i++) {
      for (j = i + 1; j < list->count; j++) {
        k = (*cmpfunc) (p_items [i]->data, p_items [j]->data);
        if (reverse)
          k *= -1;
        printf ("i=%d, j=%d, k=%d\n", i, j, k);
        if (k > 0) {
          EXCH (p_items [i], p_items [j]);
        }
      }
    }
#else
    /* qsort */
    if (reverse)
      linklist_qsort_recurse_reverse (p_items, 0, list->count - 1, cmpfunc);
    else
      linklist_qsort_recurse (p_items, 0, list->count - 1, cmpfunc);
#endif
  }
  
  list->sorted = TRUE;
  
  free (p_items);
  //_cmdline_debug ("end\n");
}


/* NOTE -- this function doesn't clear the linklist first. We just add items
 * from the array directly into the existing linklist 
 * Returns false if there are left over bytes at the end, ie. if datasize is
 * not a multiple of itemsize */
bool linklist_from_array (t_linklist *list, void *data, size_t itemsize, size_t datasize) {
  bool retval = TRUE;
  int i, j;
  char *cur;
  int count = 0;
  
  printf ("start\n");
  
  for (cur = (char *) data; cur + itemsize <= ((char *) data) + datasize; cur += itemsize) {
    linklist_add_item_copy (list, cur, itemsize);
    printf ("linklist_from_array: itemsize=%ld, count=%d\n", (long int) itemsize, count++);
  }
  
  if (cur != ((char *) data) + datasize) /* we have some left over bytes */
    retval = FALSE;
  
  printf ("end, retval=%d\n", (int) retval);
  return retval;
}


/* This one gives us a problem, in the case where linklist items are of different sizes.
 * In this case, we simply return -1 at r_itemsize
 * 
 * Another problem for C++ template classes is the following dilemma: do we just silently
 * move/copy data from one memory address to another, or do we delete the objects at old
 * addresses and recreate them in the array? I opted for the first solution (for now...)
 * though i'm pretty sure it might potentially cause trouble on non-x86/x64 architectures. */
void *linklist_to_array (t_linklist *list, size_t *r_itemsize, size_t *r_totalsize) {
  void *retval = NULL;
  int i, j;
  size_t totalsize = 0;
  t_linkitem *item;
  char *ptr;
  size_t itemsize;
  
  item = list->first;
  if (item)
    itemsize = item->size;
  else
    itemsize = 0;
  
  _cmdline_debug ("CHECKPOINT 1\n");
  
  while (item) {
    totalsize += item->size;
    if (itemsize != item->size)
      itemsize = -1;
    item = item->next;
  }
  
  _cmdline_debug ("totalsize=%ld, itemsize=%ld\n", (long int) totalsize, (long int) itemsize);
  
  if (r_itemsize)
    *r_itemsize = itemsize;
  
  if (r_totalsize)
    *r_totalsize = totalsize;
  
  retval = cmdline_malloc (totalsize);
  
  if (retval) {
    ptr = (char *) retval;
    
    item = list->first;
    while (item) {
      memcpy (ptr, item->data, item->size); /* shhhhh don't tell C++ */
      ptr += item->size;
      item = item->next;
    }
  }
  
  return retval;
}


void linklist_dealloc (t_linklist *list) {
  //t_linkitem *item, *lastitem;
  bool b;
  
  _cmdline_debug ("start, count=%d\n", list->count);
  
  /*
  item = list->first;
  list->last_accessed = list->first;
  while ((item = linklist_get_next (list)) != NULL) {
    //free (item->data);
    free (item);
  }
  */
  /*do {
    b = linklist_remove_last (list);
  } while (b);*/
  
  /*
  while (list->last) {
    _cmdline_debug ("linklist_dealloc: removing last item in list\n");
    linklist_remove_last (list);
  }*/
  linklist_remove_all (list);
  
  _cmdline_debug ("freeing list main memory block\n");
  free (list);
  
  _cmdline_debug ("end\n");
}


void linklist_inspect (t_linklist *list) {
  int i = 0;
  t_linkitem *item;
  
#ifdef WIN32
  if (sizeof (void *) == 4) /* disable this on 32bit windoze */
    return;
#endif
  
  printf ("list contents: count=%d, first=0x%lX, last=0x%lX\n",
          list->count, (long int) list->first, (long int) list->last);
  
  item = list->first;
  
  while (item) {
    printf ("  item %d: 0x%08lX (data 0x%08lX) - is copy: %s, size=%ld%s\n",
            i,
            /* item->index, */
            (long int) item,
            (long int) item->data,
            item->is_copy ? "yes" : "no",
            item->size,
            item == list->last_accessed ? " (last accessed)" : "");
    i++;
    item = item->next;
  }
  //if (!list->last_accessed)
  printf ("last_accessed is 0x%lX\n", (long int) list->last_accessed);
  printf ("\nlinklist_inspect: end\n\n");
}


#ifdef DEBUG_CMDLINE_LINKLIST

static int test_cmpfunc_strlen (void *data1, void *data2) {
  int a, b;
  a = strlen ((char *) data1);
  b = strlen ((char *) data2);
  
  //printf ("test_cmpfunc: a=%s, b=%s\n", (char *) data1, (char *) data2);
  
  //return a - b; /* FIXED - this can cause undefined results because of signed int overflow */
  if (a > b)
    return 1;
  else if (b > a)
    return -1;
  return 0;
}

static int test_cmpfunc_strcmp (void *data1, void *data2) {
  //return a - b; /* this can cause undefined results because of signed int overflow */
  return strcmp ((char *) data1, (char *) data2);
}

int linklist_test (int argc, char **argv) {
  char data1 [] = "abcdef";
  char data2 [] = "ghijklmno";
  char data3 [] = "pqrs";
  char data4 [] = "tuvwxyz";
  t_linklist *list;
  t_linkitem *item;
  char uppercase [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_LEFTOVERDATATHISSHOULDNTBEHERE";
  void *newdata;
  size_t s = 12345, ts = 123456;
  
  printf ("initializing list\n");
  list = linklist_init (FALSE);
  printf ("adding data1\n");
  linklist_add_item (list, data1, sizeof (data1));
  printf ("adding data2\n");
  linklist_add_item (list, data2, sizeof (data2));
  printf ("adding data3\n");
  linklist_add_item (list, data3, sizeof (data3));
  printf ("adding data4\n");
  linklist_add_item (list, data4, sizeof (data4));
  
  linklist_add_item (list, data1 + 1, sizeof (data1) - 1);
  printf ("adding data2\n");
  linklist_add_item (list, data2 + 1, sizeof (data2) - 1);
  printf ("adding data3\n");
  linklist_add_item (list, data3 + 1, sizeof (data3) - 1);
  printf ("adding data4\n");
  linklist_add_item (list, data4 + 1, sizeof (data4) - 1);
  
  linklist_add_item (list, data1 + 2, sizeof (data1) - 2);
  printf ("adding data2\n");
  linklist_add_item (list, data2 + 2, sizeof (data2) - 2);
  printf ("adding data3\n");
  linklist_add_item (list, data3 + 2, sizeof (data3) - 2);
  printf ("adding data4\n");
  linklist_add_item (list, data4 + 2, sizeof (data4) - 2);
  
  printf ("finding item for data3 at 0x%lX (%s)\n", (long int) data3, data3);
  item = linklist_find_item (list, data3);
  if (item) {
    printf ("result: %s\n", (char *) item->data);
    linklist_insert_item (list, item, (void *) "THIS IS INSERTED AFTER DATA3     ", -1);
  } else {
    printf ("not found\n");
  }
  
  item = linklist_find_item (list, data2);
  if (item) {
    printf ("found data2, removing from list\n");
    linklist_remove_item (list, item);
  }
  
  item = linklist_get_num (list, 4);
  if (item) {
    printf ("removing item 4 from list\n");
    //linklist_remove_item (list, item);
  } else {
    printf ("item 4 not found\n");
  }

  printf ("sorting list by string comparison\n");
  /* fugly cast, TODO: rewrite this properly */
  linklist_sort (list, (int (*)(void*, void*)) strcasecmp, FALSE);
  
  printf ("listing items\n");
  list->last_accessed = NULL;
  while ((item = linklist_get_next (list)) != NULL) {
    printf ("  found item %d, size %ld, data '%s'\n", item->index, item->size, (char *) item->data);
  }
  
  printf ("listing items backwards\n");
  list->last_accessed = NULL;
  while ((item = linklist_get_prev (list)) != NULL) {
    printf ("  found item %d, size %ld, data '%s'\n", item->index, item->size, (char *) item->data);
  }
  
  printf ("inspecting list\n");
  linklist_inspect (list);
  
  /* LIST <-> FLAT ARRAY STUFF */
  linklist_remove_all (list);
  
  linklist_add_item_copy (list, (void *) "__", 2);
  linklist_from_array (list, uppercase, 2, 26);
  
  printf ("listing items\n");
  list->last_accessed = NULL;
  while ((item = linklist_get_next (list)) != NULL) {
    printf ("  found item %d, size %ld, data '%s'\n", item->index, item->size, (char *) item->data);
  }
  
  newdata = linklist_to_array (list, &s, &ts);
  if (newdata) {
    printf ("s=%ld, ts=%ld, newdata='%s'\n", (long int) s, (long int) ts, (char *) newdata);
    free (newdata);
  }
  
  printf ("inspecting list again\n");
  linklist_inspect (list);
  
  printf ("deallocating list\n");
  linklist_dealloc (list);
  
  return 0;
}

#else

int linklist_test (int argc, char **argv) {
  // don't do anything
  return 0;
}

#endif



/****************************************************************************
 * END OF LINKLIST STUFF
 ****************************************************************************/

// we can use this now
//#define __internal_cmdline_debug cmdline_debug
/*#ifdef DEBUG_CMDLINE
#ifdef debug
#undef debug
#endif
#define debug(...) cmdline_debug(debug_color,__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifdef BP
#undef BP
#endif
#error "BREAKPOINT MACRO DEFINED"
#define BP(...) {debug(__VA_ARGS__);printf ("\x1B[1m____BREAKPOINT____\x1B[0;37m\n");getc(stdin);}
#endif*/

static void *cmdline_malloc (size_t size) {
  void *retval;
  
  retval = malloc (size);
  if (!retval) {
    printf ("cmdline_malloc: Memory allocation error (%ld bytes)\n", (long int) size);
    exit (1);
  }
  return retval;
}


static bool cmdline_is_number (char *s) {
  bool retval = TRUE;
  int i;
  
  _cmdline_debug ("start, s='%s'\n", s);
  
  for (i = 0; s [i]; i++) {
    if (!strchr (".-0123456789\n", s [i])) {
      retval = FALSE;
    }
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


static bool cmdline_is_hex (char *s) {
  bool retval = TRUE;
  int i;
  
  _cmdline_debug ("start, s='%s'\n", s);
  
  for (i = 0; s [i]; i++) {
    if (!strchr (".-x0123456789ABCDEFabcdef", s [i])) {
      retval = FALSE;
    }
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


static bool is_comment_or_empty (char *s) {
  int i = 0, j = 0;
  bool is_comment = FALSE;
  bool is_empty = TRUE;;
  
  while (s [i] == ' ' || s [i] == 9)
    i++;
  
  if (s [i] == '#')
    is_comment = TRUE;
  
  for (i = 0; s [i]; i++) {
    if (s [i] != ' ' && s [i] != 9 && s [i] != 10 && s [i] != 13)
      is_empty = FALSE;
  }
  
  return is_comment || is_empty;
}


static bool cmdline_check_minmax (float value, float min, float max) {
  bool retval = TRUE;
  
  _cmdline_debug ("start, value=%f, min=%f, max=%f\n", value, min, max);
  if (max > min) {
    if (value >= min && value <= max) {
      retval = TRUE;
    } else {
      retval = FALSE;
    }
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


/* compares the part of two strings before a = sign in the second string, and
 * after two leading dashes if present
 * eqreturn will contain the offset of the equal sign within opt, if any */
static int cmdline_strcmp (const char *str, const char *opt, long int *eqreturn) {
  long int eq = 0;
  char *eqptr;
  int retval;
  
  //_cmdline_debug ("start, str=%s, opt=%s\n", str, opt);
  
  if (!str || !opt)
    return -1;
  
  if (str [0] == '-') str++;
  if (str [0] == '-') str++;
  if (opt [0] == '-') opt++;
  if (opt [0] == '-') opt++;
  
  eqptr = strchr ((char *) opt, '=');
  if (eqptr) {
    eq = (long int) eqptr - (long int) opt;
  }
  if (eqreturn)
    *eqreturn = eq;
  
  if (eq && eq == strlen (str))
    retval = strncmp (str, opt, eq);
  else
    retval = strcmp (str, opt);
  
  //_cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


/* returns true if the short name character matches, or if
 * the longname string matches excluding leading 2 dashes if present */
static bool opt_match (t_opt *opt, char shortname, char *longname) {
  bool retval = FALSE;
  
  /*_cmdline_debug ("opt_match: start, shortname=%c, longname='%s', opt->shortname=%c, opt->longname=%s\n",
         shortname, longname, opt->shortname, opt->longname);*/
  if (opt) {
    if (opt->shortname == shortname && shortname != ' ' && shortname)
      retval = TRUE;
    if (/*opt->longname && */longname) {
      if (!cmdline_strcmp (opt->longname, longname, NULL)) {
        retval = TRUE;
      }
    }
  } else {
    _cmdline_debug ("got null pointer!\n");
  }
  
  //_cmdline_debug ("opt_match: end, retval=%d\n", retval);
  return retval;
}


static int get_longname_from_origname (char *longname, char *orig, int max) {
  int i, retval = 0;
  
  _cmdline_debug ("start, orig=%s, max=%d\n", orig, max);
  
  if (orig [0] == '-') orig++;
  if (orig [0] == '-') orig++;
  
  i = 0;
  while (orig [i] != 0 && orig [i] != '=' && orig [i] >= ' ' && i < max) {
    longname [i] = orig [i];
    i++;
  }
  longname [i] = 0;
  
  _cmdline_debug ("end, longname=%s\n", longname);
  return retval;
}


void cmdline_remove_specific (t_cmdline *data, t_linkitem *item) {
  t_opt *opt;
  
  if (!item)
    return;
  
  opt = (t_opt *) item->data;
  
  // dealloc this opt
  if (opt) {
    if (opt->string_value) {
      free (opt->string_value);
      opt->string_value = NULL;
    }
    opt->shortname = 0;
    opt->longname [0] = 0;
    
    if (opt->strlist) {
      linklist_dealloc (opt->strlist);
      opt->strlist = NULL;
    }
  }
  
  // then remove it from the list
  linklist_remove_item (data->opts, item);
}


void cmdline_remove (t_cmdline *data, char shortname, char *longname) {
  t_linkitem *item;
  
  item = cmdline_find_opt_linkitem (data, shortname, longname);
  if (item) {
    cmdline_remove_specific (data, item);
  }
}


void cmdline_dealloc (t_cmdline *data) {
  t_opt *opt, *lastopt;
  t_linkitem *item = NULL;
  
  _cmdline_debug ("start\n");
  
  while ((item = linklist_get_next (data->opts)) != NULL) {
    opt = (t_opt *) item->data;
    if (opt->string_value)
      free (opt->string_value);
    //free (opt);
  }

  if (data->opts)
    linklist_dealloc (data->opts);
  
  if (data->extra_args)
    linklist_dealloc (data->extra_args);
  
  free (data);
  _cmdline_debug ("end\n");
}


static t_linkitem *cmdline_find_opt_linkitem (t_cmdline *data, char shortname, char *longname) {
  t_linkitem *item;
  bool found = FALSE;
  
  /*_cmdline_debug ("start, shortname=%c, longname=%s\n", shortname, longname);*/
  
  data->opts->last_accessed = NULL;
  while (!found && ((item = linklist_get_next (data->opts))) != NULL) {
    if (opt_match ((t_opt *) item->data, shortname, longname)) {
      found = TRUE;
      // retval = (t_opt *) item->data;
    }
  }
  
  /*_cmdline_debug ("end, shortname=%c, longname=%s retval=0x%lX\n",
         shortname, longname, (long int) retval);*/
  return item; //retval;
}


static t_opt *cmdline_find_opt (t_cmdline *data, char shortname, char *longname) {
  t_linkitem *item = cmdline_find_opt_linkitem (data, shortname, longname);
  
  if (item)
    return (t_opt *) item->data;
  else
    return NULL;
}


static t_opt *cmdline_add_opt (t_cmdline *data, char shortname, char *longname) {
  char **argv = NULL;
  int argc = 0;
  t_opt *retval = NULL, newopt;
  t_linkitem *item;
  
  _cmdline_debug ("start, shortname=%c, longname='%s'\n", shortname, longname);
  
  switch (data->parsing_mode) {
    case CMDLINE_MODE_CMDLINE:
      _cmdline_debug ("CMDLINE_MODE_CMDLINE\n");
      argv = data->argv;
    break;
    case CMDLINE_MODE_ENV:
      _cmdline_debug ("CMDLINE_MODE_ENV\n");
      argv = data->argv_string;
    break;
    case CMDLINE_MODE_FILE:
      _cmdline_debug ("CMDLINE_MODE_FILE\n");
      argv = data->argv_file;
    break;
  }
  
  /*if (!argv) {
    _cmdline_debug ("cmdline_add_opt: !argv\n");
    return NULL;
  }*/
  
  if (data->i >= CMDLINE_MAX_OPTS) {
    _cmdline_debug ("maximum number of options reached\n");
    return NULL;
  } else {
    _cmdline_debug ("adding opt struct to link list\n");
    item = linklist_add_item_copy (data->opts, &newopt, sizeof (t_opt));
    retval = (t_opt *) item->data;
    retval->errorcode = CMDLINE_ERROR_NOERROR;
    retval->type = CMD_VOID;
    retval->unknown = TRUE;
    retval->lookup_count = FALSE;
    retval->longname [0] = 0;
    retval->min = 0;
    retval->max = 0;
    retval->num_occurrences = 0;
    retval->shortname = shortname;
    retval->argv_index = (data->parsing_mode == CMDLINE_MODE_CMDLINE) ? data->i : -1;
    retval->index = data->count;
    retval->string_value = NULL;
    retval->from_cmdline = FALSE;
    retval->from_env = FALSE;
    retval->from_file = FALSE;
    retval->strlist = NULL;
    
    if (!data->parsing_multiple && argv && argv [data->i]) {
      _cmdline_debug ("handling string '%s'\n", argv [data->i]);
      _cmdline_debug ("allocating memory for longname\n");
      get_longname_from_origname (retval->longname, 
                argv [data->i], CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH);
    }
  }
  data->count++;
  //retval = newopt;
  
  _cmdline_debug ("end, shortname=%c, longname=%s retval=0x%lX\n",
         shortname, longname, (long int) retval);
  return retval;
}


static t_opt *cmdline_find_or_add_opt (t_cmdline *data, char shortname, char *longname) {
  t_opt *retval;
  _cmdline_debug ("start, shortname=%c, longname='%s'\n", shortname, longname);
  
  retval = cmdline_find_opt (data, shortname, longname);
  if (!retval) {
    retval = cmdline_add_opt (data, shortname, longname);
  }
  
  _cmdline_debug ("end, shortname=%c, longname=%s retval=0x%lX\n",
         shortname, longname, (long int) retval);
  return retval;
}


static int cmdline_find_argsetup (t_cmdline *data, char shortname, char *longname) {
  bool argsetup_found = FALSE;
  int s = 0;
  long int eqret;
  
  while (!argsetup_found && data->argsetup [s].type != CMD_NONE) {
    if (data->argsetup [s].shortname == shortname && shortname != ' ' && shortname)
      argsetup_found = TRUE;
    if (data->argsetup [s].longname && longname) {
      if (!cmdline_strcmp (data->argsetup [s].longname, longname, &eqret)) {
        argsetup_found = TRUE; //data->count;
      }
    }
    if (!argsetup_found) {
      s++;
    }
  }
  if (argsetup_found)
    return s;
  else
    return -1;
}


/* return value: TRUE if we should skip next arg */
static bool cmdline_parse_single (t_cmdline *data, char shortname, char *longname, char *next) {
  int optindex = 0, i = 0, l = 0, s = 0, count = 0;
  long int eqret = 0, eq = 0;
  bool argsetup_found = FALSE, have_eq = FALSE, retval = FALSE;
  float temp;
  t_opt *opt;
  bool get_string_value = TRUE;
  int overwrite_mask = CMDLINE_NO_OPTS;
  char empty = 0;
  
  _cmdline_debug ("start\n");
  
  if (longname)
    eq = (long int) strchr (longname, '=');
  if (eq)
    eq -= (long int) longname;
  if (eq) {
    next = longname + eq + 1;
    have_eq = TRUE;
  }
  
  _cmdline_debug ("parsing single option shortname=%c, longname=%s, eq=%ld, next arg=%s\n",
          shortname, longname, eq, next);
  
  if (longname && !strcmp (longname, "--")) {
    data->doubledash = data->i;
    return 0;
  }
  
  s = cmdline_find_argsetup (data, shortname, longname);
  if (s >= 0)
    argsetup_found = TRUE;
  
  opt = cmdline_find_opt (data, shortname, longname);
  if (opt) {
    if (opt->from_cmdline)
      overwrite_mask = CMDLINE_CMDLINE_OPTS;
    if (opt->from_env)
      overwrite_mask = CMDLINE_ENV_OPTS;
    if (opt->from_file)
      overwrite_mask = CMDLINE_FILE_OPTS;
  }
  
  if (data->overwrite_flags & overwrite_mask) {
    opt = cmdline_find_or_add_opt (data, shortname, longname);
  } else {
    if (opt/*cmdline_find_opt (data, shortname, longname)*/) {
      return FALSE;
    } else {
      opt = cmdline_add_opt (data, shortname, longname);
    }
  }
  
  if (!opt) {
    printf ("Memory allocation error\n");
    return 0;
  }
  
  switch (data->parsing_mode) {
    case CMDLINE_MODE_CMDLINE:
      opt->from_cmdline = TRUE;
    break;
    case CMDLINE_MODE_ENV:
      opt->from_env = TRUE;
    break;
    case CMDLINE_MODE_FILE:
      opt->from_file = TRUE;
    break;
  }
  
  opt->num_occurrences++;
  
  for (i = 0; i < CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH; i++)
    if (opt->longname [i] < ' ')
      opt->longname [i] = 0;
  
  /*if (shortname != ' ' && shortname != 0)
    opt->shortname = shortname;*/
  //opt->orig = longname;
  opt->unknown = !argsetup_found;
  if (opt->unknown)
    data->unknown_args++;
  
  if (argsetup_found) {
    _cmdline_debug ("found argsetup %d: %c, %s, min=%f, max=%f\n",
           s,
           data->argsetup [s].shortname,
           data->argsetup [s].longname,
           data->argsetup [s].min,
           data->argsetup [s].max);

    if (!have_eq)
      retval = TRUE;
    opt->type = data->argsetup [s].type & 0xFF;//(~CMDLINE_WRITE_TO_CFG_FLAG);
    //printf ("\n\ntype=0x%x, from argsetup=0x%x\n\n\n", opt->type, data->argsetup [s].type);
    opt->write_to_file = (data->argsetup [s].type & CMDLINE_WRITE_TO_CFG_FLAG) ? TRUE : FALSE;
    opt->min = data->argsetup [s].min;
    opt->max = data->argsetup [s].max;
    opt->shortname = data->argsetup [s].shortname;
    strncpy (opt->longname, data->argsetup [s].longname, CMDLINE_LONGNAME_CMDLINE_MAX_LENGTH);
    
    //BP ("type=0x%x", (int) data->argsetup [s].type);
    //switch (data->argsetup [s].type) { // FIXED IT!!!!!!!!
    switch (opt->type) {
      case CMD_BOOL:
        opt->num_value = 1;
        retval = FALSE;
        if (have_eq) {
          char buf [16];
          strncpy (buf, next, 15);
          for (i = 0; i < 15; i++) {
            if (buf [i] < ' ') {
              buf [i] = 0;
            }
          }
          if (!strcasecmp (buf, "true") || !strcasecmp (buf, "yes") || !strcasecmp (buf, "1")) {
            opt->num_value = 1;
          } else if (!strcasecmp (buf, "false") || !strcasecmp (buf, "no") || !strcasecmp (buf, "0")) {
            opt->num_value = 0;
          } else {
            //opt->errorcode = CMDLINE_ERROR_NOTBOOL;
            opt->num_value = 0;
          }
        }
      break;
      
      //case CMD_VOID:
      case CMD_COUNT:
        retval = FALSE;
        if (next && have_eq) {
          temp = strtof (next, NULL);
          if (!cmdline_is_number (next) || !cmdline_check_minmax (temp, data->argsetup [s].min, data->argsetup [s].max)) {
            _cmdline_debug ("error: value %s out of bounds (min=%f, max=%f)\n", next, data->argsetup [s].min, data->argsetup [s].max);
            data->errors++;
            opt->errorcode = CMDLINE_ERROR_BOUNDS;
          }
          opt->num_occurrences = (int) temp;
        }
      break;
      
      case CMD_STRING:
        if (next) {
          _cmdline_debug ("string value found for argument %c (%s)\n",
                  shortname, longname);
          //opt->string_value = next;
          get_string_value = TRUE;
          
          /* SLIGHT CHANGE IN API BEHAVIOUR:
          num_value of options marked as type 'string' will simply be the option's
          string length, instead of trying to parse its actual numerical value. */
           
          //opt->num_value = strtof (next, NULL);
          opt->num_value = strlen (next);
          cmdline_opt_inspect (opt);
          //BP ("next='%s', opt->num_value=%f", next, opt->num_value);
        } else {
          _cmdline_debug ("string value not found for argument %c (%s)\n",
                  shortname, longname);
          data->errors++;
          opt->errorcode = CMDLINE_ERROR_NOARG;
        }
      break;
      
      case CMD_STRLIST:
        if (next) {
          //opt->string_value = next;
          get_string_value = FALSE;
        } else {
          _cmdline_debug ("string value not found for argument %c (%s)\n",
                  shortname, longname);
          data->errors++;
          opt->errorcode = CMDLINE_ERROR_NOARG;
        }
      break;
      
      case CMD_VOID:
        if (next)
          temp = strtof (next, NULL);
        else
          temp = 1;
        
        opt->num_value = temp;
        get_string_value = TRUE;
      break;
      
      case CMD_INT:
      case CMD_LONGINT:
      case CMD_FLOAT:
        if (next) {
          temp = strtof (next, NULL);
          if (!cmdline_is_number (next)) {
            _cmdline_debug ("error: value '%s' is not a number\n", next);
            data->errors++;
            opt->errorcode = CMDLINE_ERROR_NAN;
          }
          if (!cmdline_check_minmax (temp, data->argsetup [s].min, data->argsetup [s].max)) {
            _cmdline_debug ("error: value '%s' out of bounds (min=%f, max=%f)\n",
                   next, data->argsetup [s].min, data->argsetup [s].max);
            data->errors++;
            opt->errorcode = CMDLINE_ERROR_BOUNDS;
          }
          opt->num_value = temp;
          //BP ("next='%s', opt->num_value=%f", next, opt->num_value);
          //opt->string_value = next;
          get_string_value = TRUE;
        } else {
          _cmdline_debug ("error: number value not found for argument %c (%s)\n",
                  shortname, longname);
          data->errors++;
          opt->errorcode = CMDLINE_ERROR_NOARG;
        }
      break;
      
      case CMD_NONE:
      default:
        _cmdline_debug ("this shouldn't happen\n");
      break;
    }
  } else { /* !argsetup_found */
    opt->unknown = TRUE;
    opt->min = 0;
    opt->max = 0;
    if (next) {
      //opt->string_value = next;
      get_string_value = TRUE;
      opt->num_value = strtof (next, NULL);
    }
  }
  
  if (opt->type == CMD_STRLIST) {
    char *str = next ? next : &empty;
    _cmdline_debug ("adding to strlist of %c(%s): string='%s'\n", opt->shortname, opt->longname, str);
    cmdline_add_to_strlist (opt, str, strlen (str) + 4);
  } else {
    //if (/*get_string_value && next && */ !opt->string_value) {
    if (get_string_value) {
      int len;
      char *str = next ? next : &empty;
      
      _cmdline_debug ("getting string value for option %c(%s): %s\n",
             shortname, longname, str);
             
      if (next && opt->string_value) {
        free (opt->string_value);
      }
      
      len = strlen (str);
      //if (opt->string_value) // see if ( /* ... && */ !opt->string_value) above
        //free (opt->string_value);
      _cmdline_debug ("allocating memory\n");
      opt->string_value = (char *) cmdline_malloc (len + 4);
      strcpy (opt->string_value, str);
      /* try to do this in a portable way that doesn't f'up unicode */
      for (i = 0; i < len; i++) {
        if (opt->string_value [i] == 0x0A || opt->string_value [i] == 0x0D) {
          opt->string_value [i] = 0;
        }
      }
    }
  }
  
  //data->count++;
  _cmdline_debug ("result: shortname %c, longname '%s'\n",
         opt->shortname, opt->longname);
  _cmdline_debug ("end, shortname=%c, longname='%s', count=%d\n",
         shortname, longname, data->count);
  return retval;
}


/* return value: TRUE if we should skip next arg */
static bool cmdline_parse_multiple (t_cmdline *data, char *opts, char *next) {
  bool retval = 0;
  int i, len, eq = 0, s = 0, bitfield = 0;
  char *c;
  
  _cmdline_debug ("start\n");
  
  if (!opts)
    return 0;
  
  c = strchr (opts, '=');
  if (c)
    eq = c - opts;
  
  s = cmdline_find_argsetup (data, ' ', opts + 1);
  
  if (eq) bitfield |= 1;
  if (s >= 0) bitfield |= 2;
  
  _cmdline_debug ("\n\neq=%d\n\n", eq);
  switch (bitfield) {
    case 0: /* no equal sign, no argsetup */
      /* treat each character except for the first (dash) as a short option */
      len = strlen (opts);
      for (i = 1; i < len; i++) {
        if (i == len - 1) {
          data->parsing_multiple = TRUE;
          retval = cmdline_parse_single (data, opts [i], NULL, next);
        } else {
          data->parsing_multiple = TRUE;
          retval = cmdline_parse_single (data, opts [i], NULL, NULL);
        }
      }
    break;
    
    case 1: /* equal sign found, no argsetup */
      data->parsing_multiple = FALSE;
      if (eq == 2)
        retval = cmdline_parse_single (data, opts [1], NULL, opts + eq);
      else
        retval = cmdline_parse_single (data, ' ', opts + 1, opts + eq);
    break;

    case 2: /* no equal sign found, argsetup found */
      data->parsing_multiple = FALSE;
      retval = cmdline_parse_single (data, ' ', opts + 1, next);
    break;
    
    case 3: /* equal sign found, argsetup found */
      data->parsing_multiple = FALSE;
      retval = cmdline_parse_single (data, ' ', opts + 1, opts + eq);
    break;
    
    default:
      printf ("this shouldn't happen.\n");
    break;
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


static int cmdline_parse_array (t_cmdline *data, int argc, char **argv) {
  int i, j, retval = 0, oldcount = 0, start = 0;
  char *nextarg;
  bool from_file = (data->parsing_mode == CMDLINE_MODE_FILE);
  
  _cmdline_debug ("start, argc=%d\n", argc);
  oldcount = data->count;
  
  if (argc <= 0 || !argv)
    return 0;
  
  switch (data->parsing_mode) {
    case CMDLINE_MODE_CMDLINE:
      start = 1;
      data->argv = argv;
    break;
    case CMDLINE_MODE_ENV:
      data->argv_string = argv;
    break;
    case CMDLINE_MODE_FILE:
      data->argv_file = argv;
    break;
  }
  
  for (data->i = start; data->i < argc; data->i++) {
    if (data->i == argc - 1)
      nextarg = NULL;
    else
      nextarg = argv [data->i + 1];
      
    if ((argv [data->i] [0] == '-' && !data->doubledash) || from_file) {
      if (argv [data->i] [1] == '-' || from_file) {
        data->parsing_multiple = FALSE;
        if (cmdline_parse_single (data, ' ', argv [data->i], nextarg)) {
          data->i++;
        }
      } else {
        data->parsing_multiple = TRUE;
        if (strlen (argv [data->i]) == 2) {
          if (cmdline_parse_single (data, argv [data->i] [1], NULL, nextarg)) {
            data->i++;
          }
        } else {
          if (cmdline_parse_multiple (data, argv [data->i], nextarg)) {
            data->i++;
          }
        }
      }
    } else {
      /* add argument to extra_args */
      add_extra_arg (data, argv [data->i]);
    }
  }
  
  retval = data->count - oldcount;
  _cmdline_debug ("end, retval=%d", retval);
  return retval;
}


static bool add_extra_arg (t_cmdline *data, char *name) {
  bool retval = FALSE;
  
  if (data->extra_args->count < data->extra_args_max || data->extra_args_max < 0) {
    _cmdline_debug ("adding argument '%s' as extra arg %d\n", name, data->extra_args->count);
    linklist_add_item_copy (data->extra_args, name, strlen (name) + 4);
    /*data->extra_args [data->extra_args->count] = name;
     * data->count++;
     * data->extra_args_count++;*/
    retval = TRUE;
  } else {
    _cmdline_debug ("max number of extra args reached, ignoring argument '%s'\n", name);
    data->errors++;
    retval = FALSE;
  }
  return retval;
}


t_cmdline *cmdline_parse (t_argsetup *argsetup, 
                          int argc, char **argv, int extra_args_max) {
  int i, j;
  t_cmdline *data;
  static t_argsetup empty_argsetup [] = {
    { 0, NULL, CMD_NONE, 0, 0 }
  };
  
  _cmdline_debug ("start, argc=%d\n", argc);
  _cmdline_debug ("allocating memory\n");
  data = (t_cmdline *) cmdline_malloc (sizeof (t_cmdline));
  
  if (!argsetup)
    argsetup = empty_argsetup;
  
  data->argc = argc;
  data->argv = argv;
  data->argv_string = NULL;
  data->argv_file = NULL;
  data->argsetup = argsetup;
  data->argv0 = argv [0];
  data->errors = 0;
  data->unknown_args = 0;
  data->doubledash = 0;
  data->count = 0;
  data->overwrite_flags = CMDLINE_ALL_OPTS;
  data->extra_args_max = extra_args_max;
  data->parsing_mode = CMDLINE_MODE_CMDLINE;
  data->opts = linklist_init (TRUE);
  data->extra_args = linklist_init (TRUE);
  
  cmdline_parse_array (data, argc, argv);
  
  _cmdline_debug ("end\n");
  return data;
}


int cmdline_parse_string (t_cmdline *data, t_argsetup *argsetup, char *str, int overwrite_flags) {
  int i, j, len, retval = 0, cmdcount = 0;
  char *buffer;
  char *cmd [CMDLINE_MAX_OPTS]; /* should be reasonable */
  char **new_extra_args;
  char empty = 0;
  static t_argsetup empty_argsetup [] = {
    { 0, NULL, CMD_NONE, 0, 0 }
  };
  
  _cmdline_debug ("start, str='%s'\n", str);
  
  if (!data)
    return 0;
  
  data->overwrite_flags = overwrite_flags;

  if (!argsetup)
    argsetup = empty_argsetup;
  
  len = strlen (str);
  _cmdline_debug ("allocating memory\n");
  buffer = (char *) cmdline_malloc (len + 4);
  
  for (i = 0; i < CMDLINE_MAX_OPTS; i++)
    cmd [i] = &empty;
  
  strcpy (buffer, str);
  cmd [0] = buffer;
  
  //len = strlen (buffer);
  for (i = 0; buffer [i]; i++) {
    if (buffer [i] == 10 || buffer [i] == 13) {
      buffer [i] = 0;
      len = i;
      break;
    }
  }

  j = 0;
  while (buffer [j] == ' ') /* skip spaces */
    j++;
  
  for (i = 1; i < CMDLINE_MAX_OPTS; i++) {
    
    /* find next space or end of string */
    while (buffer [j] != ' ' && j < len)
      j++;
    /* skip multiple spaces up to next arg */
    while (buffer [j] == ' ' && j < len) {
      buffer [j] = 0;
      j++;
    }
    
    if (buffer [j]) {
      cmd [i] = buffer + j;
      cmdcount++;
    } else {
      //cmd [i] = NULL;
      i = CMDLINE_MAX_OPTS;
    }
  }
  if (cmd [0] [0])
    cmdcount++;
  
  _cmdline_debug ("cmdcount=%d\n", cmdcount);
  i = 0;
  /*while (cmd [i] && i < CMDLINE_MAX_OPTS) {
    _cmdline_debug ("cmd [%d] = '%s'\n", i, cmd [i]);
    i++;
  }*/
  for (i = 0; i < cmdcount; i++)
    _cmdline_debug ("cmd [%d] = '%s'\n", i, cmd [i]);
  
  //data->argv_string = cmd;
  cmdline_parse_array (data, cmdcount, cmd);
  
  free (buffer);
  
  _cmdline_debug ("end, returning cmdcount=%d\n", cmdcount);
  return cmdcount;
}


int cmdline_parse_env (t_cmdline *data, t_argsetup *argsetup, char *env, int overwrite_flags) {
  char *value;
  int doubledash;
  int retval;
  int old_parsing_mode;
  
  _cmdline_debug ("start\n");
  
  if (!data)
    return 0;
  
  old_parsing_mode = data->parsing_mode;
  doubledash = data->doubledash;
  data->doubledash = 0;
  data->parsing_mode = CMDLINE_MODE_ENV;
  
  value = getenv (env);
  if (!value) {
    _cmdline_debug ("Environment variable %s not found, returning 0\n", env);
    return 0;
  }
  
  _cmdline_debug ("Got environmnet variable %s: '%s'\n", env, value);
  
  retval = cmdline_parse_string (data, argsetup, value, overwrite_flags);
  data->doubledash = doubledash;
  
  data->parsing_mode = old_parsing_mode;
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


int cmdline_parse_file (t_cmdline *data, t_argsetup *argsetup, char *filename, int overwrite_flags) {
  int retval = 0, doubledash;
  FILE *f = NULL;
  char buffer [CMDLINE_PATH_MAX + CMDLINE_MAX_LENGTH + 1];
  static char *stringarray [1];
  struct stat filestat;
  
  _cmdline_debug ("start, filename='%s'\n", filename);
  
  if (!data || !filename)
    return -1;
  
  data->overwrite_flags = overwrite_flags;
  
  doubledash = data->doubledash;
  data->doubledash = 0;
  
  if (!stat (filename, &filestat))
    f = fopen (filename, "r");
  
  if (!f) {
    _cmdline_debug ("couldn't open file '%s'\n", filename);
  } else {
    int old_parsing_mode = data->parsing_mode;
    data->parsing_mode = CMDLINE_MODE_FILE;
    stringarray [0] = buffer;
    data->argv_file = stringarray;
    
    while (fgets (buffer, CMDLINE_PATH_MAX + CMDLINE_MAX_LENGTH, f))
      if (!is_comment_or_empty (buffer))
        retval += cmdline_parse_array (data, 1, stringarray);
  
    fclose (f);
    data->parsing_mode = old_parsing_mode;
  }
  
  data->doubledash = doubledash;
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


static bool file_write_one_entry (FILE *f, t_opt *opt) {
  int len = 0, count = 0;
  size_t s;
  char buf [CMDLINE_PATH_MAX + CMDLINE_MAX_LENGTH + 1];
  char *ptr;
  
  _cmdline_debug ("start, opt=%c,'%s' type=%d\n", opt->shortname, opt->longname, opt->type);
  
  if (/*!opt->longname || */!opt->longname [0])
    return FALSE;
  
  switch (opt->type) {
    case CMD_VOID:
      len = sprintf (buf, "%s\n", opt->longname);
    break;
        
    case CMD_STRING:
      len = sprintf (buf, "%s=%s\n", opt->longname, opt->string_value);
    break;
        
    case CMD_BOOL:
      len = sprintf (buf, "%s=%s\n", opt->longname, opt->num_value ? "true" : "false");
    break;
        
    case CMD_INT:
    case CMD_LONGINT:
      len = sprintf (buf, "%s=%ld\n", opt->longname, (long int) opt->num_value);
    break;
        
    case CMD_FLOAT:
      len = sprintf (buf, "%s=%f\n", opt->longname, opt->num_value);
    break;
        
    case CMD_COUNT:
      count = opt->num_occurrences;
      if (cmdline_check_minmax (count, opt->min, opt->max))
        len = sprintf (buf, "%s=%d\n", opt->longname, opt->num_occurrences);
    break;
        
    case CMD_NONE:
    default:
      len = 0;
      printf ("this shouldn't happen\n");
    break;
    
    case CMD_STRLIST:
      printf ("cmd_strlist isn't being written to files (as of yet)\n");
      _cmdline_debug ("cmd_strlist isn't being written to files (as of yet)\n");
    break;
  }
  if (len > 2) {
    ptr = buf;
    if (buf [0] == '-' && buf [1] == '-') {
      ptr += 2;
      len -= 2;
    }
    _cmdline_debug ("writing buffer length %d: %s\n", len, ptr);
    s = fwrite (ptr, 1, len, f);
    /* TODO: check write success/fail */
  }
  return TRUE;
}


static int compare_two_opts (t_opt *opt1, t_opt *opt2) {
  int retval = 0;
  
  retval = cmdline_strcmp (opt1->longname, opt2->longname, NULL);
  return retval;
}


static int cmp_longname (void *a, void *b) {
  t_opt *opt_a = (t_opt *) a;
  t_opt *opt_b = (t_opt *) b;
  
  _cmdline_debug ("start\n");
  
  return cmdline_strcmp (opt_a->longname, opt_b->longname, NULL);
  
  _cmdline_debug ("end\n");
}


int cmdline_args_to_write_to_file (t_cmdline *data) {
  t_opt *opt;
  t_linkitem *item;
  int retval = 0;
  
  _cmdline_debug ("start\n");
  
  if (!data || !data->opts) {
    _cmdline_debug ("!data || !data->opts -- aborting!!\n");
    return -1;
  }
  
  item = data->opts->first;
  
  while (item) {
    opt = (t_opt *) item->data;
    if (opt->write_to_file) {
      //_cmdline_debug ("found arg with write_to_file set\n");
      retval++;
    }
    item = item->next;
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


/* TODO: decide what to do with string arrays (type 'strlist') */
/* TODO: support section names between [ ] */
int cmdline_write_file (t_cmdline *data, char *filename, int overwrite_mode) {
  int i = 0, j = 0, len = 0, count = 0, retval = 0;
  FILE *f = NULL;
  struct stat filestat;
  bool already_exists;
  t_opt *opt, *optswap;
  t_opt **opttable;
  t_linkitem *item;
  
  if (!data || !filename)
    return 0;
  
  //BP ("list count=%d", data->opts->count);
  _cmdline_debug ("checking '%s'\n", filename);
  
  already_exists = !stat (filename, &filestat);
  
  /*if (!already_exists || overwrite) {
    _cmdline_debug ("opening '%s'\n", filename);
    f = fopen (filename, "w");
  }*/
  
  switch (overwrite_mode) {
    case CMDLINE_ABORT:
      if (!already_exists)
        f = fopen (filename, "w");
    break;
    
    case CMDLINE_OVERWRITE:
      f = fopen (filename, "w");
    break;
    
    case CMDLINE_APPEND:
      f = fopen (filename, "a");
    break;
  }
  
  if (!f) {
    _cmdline_debug ("error opting '%s'\n", filename);
    return -1;
  }
  
#ifdef USE_QSORT
  linklist_sort (data->opts, cmp_longname, FALSE);
#endif
  
  /* create an array of known options */
  data->opts->last_accessed = NULL;
  printf ("allocating memory\n");
  opttable = (t_opt **) cmdline_malloc (data->count * sizeof (t_opt *));
  
  while ((item = linklist_get_next (data->opts)) != NULL) {
    opt = (t_opt *) item->data;
    cmdline_opt_inspect (opt);
    //BP ("opt->unknown=%d, opt->errorcode=%d", (int) opt->unknown, (int) opt->errorcode);
    if (opt && opt->write_to_file && !opt->unknown && !opt->errorcode) {
      opttable [count] = opt;
      count++;
    }
  }
  
  /* sort the array */
  /* DONE: implement linklist_sort () using a qsort algo */
  /* DONE: use linklist_sort here. */
#ifndef USE_QSORT
  for (i = 0; i < count; i++) {
    for (j = i; j < count; j++) {
      if (compare_two_opts (opttable [i], opttable [j]) > 0) {
        optswap = opttable [i];
        opttable [i] = opttable [j];
        opttable [j] = optswap;
      }
    }
  }
#endif
  //BP ("count=%d", count);

  /* finally, write the entries to file */
  for (i = 0; i < count; i++) {
    if (/*opt->write_to_file &&*/ file_write_one_entry (f, opttable [i])) {
      retval++;
    }
  }
  
  free (opttable);
  fclose (f);
  
  _cmdline_debug ("retval=%d", retval);
  return retval;
}


int cmdline_print_errors (t_cmdline *data,
                          FILE *out,
                          bool include_unknown_args,
                          bool include_extra_args) {
  t_linkitem *item;
  int i, retval = 0;
  char *str;
  t_opt *opt;
  static const char *errorstrings [] = {
    "no error",
    "value out of bounds",
    "value must be a number",
    "bogus boolean value",
    "value not found"
  };
  
  _cmdline_debug ("start\n");
  
  data->opts->last_accessed = NULL;
  while ((item = linklist_get_next (data->opts)) != NULL) {
    opt = (t_opt *) item->data;
    if (opt && opt->errorcode != CMDLINE_ERROR_NOERROR) {
      fprintf (out, "Error parsing option -%c %s: %s\n",
               opt->shortname,
               opt->longname,
               errorstrings [opt->errorcode]);
      _cmdline_debug ("checkpoint 1, errorcode=%d\n", opt->errorcode);
      retval++;
    } else if (include_unknown_args &&
                   opt && opt->unknown && opt->lookup_count == 0) {
      char shortstr [8] = "";
      if (opt->shortname != ' ' && opt->shortname > 32)
        sprintf (shortstr, "%c ", opt->shortname);
      fprintf (out, "Unknown option - %s%s\n",
               shortstr,
               opt->longname/* ? opt->longname : ""*/);
      _cmdline_debug ("checkpoint 2, errorcode=%d\n", opt->errorcode);
      retval++;
    }
  }
  
  if (include_extra_args) {
    data->extra_args->last_accessed = NULL;
    while ((item = linklist_get_next (data->extra_args)) != NULL) {
      str = (char *) item->data;
      fprintf (out, "Unknown option - %s\n", str);
      _cmdline_debug ("checkpoint 3\n");
      retval++;
    }
  }
  
  _cmdline_debug ("end, retval=%d\n", retval);
  return retval;
}


void cmdline_opt_inspect (t_opt *opt) {
  _cmdline_debug ("start\n");
  printf (
          "  argv_index=%d\n"
          "  shortname=%c\n"
          "  longname=%s\n"
          "  min=%f\n"
          "  max=%f\n"
          "  num_occurrences=%d\n"
          "  num_value=%f\n"
          "  string_value='%s'\n"
          "  from_cmdline=%d\n"
          "  from_env=%d\n"
          "  from_file=%d\n\n"
          ,
          opt->argv_index,
          opt->shortname,
          opt->longname,
          opt->min,
          opt->max,
          opt->num_occurrences,
          opt->num_value,
          opt->string_value,
          opt->from_cmdline,
          opt->from_env,
          opt->from_file
         );
  _cmdline_debug ("end\n");
}


void cmdline_inspect (t_cmdline *cmdline) {
  int i = 0;
  t_linkitem *item = NULL;
  
  _cmdline_debug ("start\n");
  
  printf ("  argc=%d\n"
          "  argv0=%s\n"
          "  count=%d\n"
          "  i=%d\n"
          "  errors=%d\n"
          "  unknown_args=%d\n"
          "  extra_args_max=%d\n"
          "  doubledash=%d\n\n",
          cmdline->argc,
          cmdline->argv0,
          cmdline->count,
          cmdline->i,
          cmdline->errors,
          cmdline->unknown_args,
          cmdline->extra_args_max,
          cmdline->doubledash);
  
  printf ("\nlist of options:\n");
  cmdline->opts->last_accessed = NULL;
  //_cmdline_debug ("checkpoint 1\n");
  while ((item = linklist_get_next (cmdline->opts)) != NULL) {
    t_opt *opt = (t_opt *) item->data;
    //_cmdline_debug ("checkpoint 2\n");
    if (opt) {
      //_cmdline_debug ("checkpoint 3\n");
      printf ("  opt %d(%d) type %x: %c (%s) - num_value=%f, string_value='%s', min=%f, max=%f\n",
              item->index,
              opt->index,
              opt->type,
              opt->shortname,
              opt->longname,
              opt->num_value,
              opt->string_value,
              opt->min,
              opt->max);
      if (opt->type == CMD_STRLIST) {
        if (opt->strlist) {
          //printf ("  type CMD_STRLIST found, count=%d\n", opt->strlist->count);
          t_linkitem *item = opt->strlist->first;
          while (item) {
            printf ("    strlist item: %s\n", (char *) item->data);
            item = item->next;
          }
        } else {
          printf ("opt->strlist == NULL!!\n");
        }
      }
    } else {
      printf ("opt is null pointer!\n");
    }
  }
  
  //_cmdline_debug ("checkpoint 4\n");
  printf ("\nextra args:\n");
  /*for (i = 0; i < cmdline->extra_args_count; i++) {
    printf ("  extra arg %d: '%s'\n", i, cmdline->extra_args [i]);
  }*/
  
  cmdline->extra_args->last_accessed = NULL;
  while ((item = linklist_get_next (cmdline->extra_args)) != NULL) {
    char *str = (char *) item->data;
    printf ("  extra arg %d: '%s'\n", item->index, str);
  }
  
  printf ("\ncmdline_inspect: end\n\n");
}

//#ifdef CMDLINE_TEST
#ifdef DEBUG_CMDLINE

/*#ifdef __cplusplus
extern "C" {
#endif*/

/* example usage and testing */

int cmdline_test (int argc, char **argv) {
  t_cmdline *cmdline;
  int i, c, v, envnum, filenum;
  long int l;
  int test_int = 45;
  char test_str [16] = "test string", *pchar;
  float test_float = 6.789;
  t_opt *opt;
  t_linkitem *item;
  t_linklist *list;
  
  static t_argsetup argsetup [] = {
    /*  short  long               type              min     max */
    {   'i',   "--test-int",      CMD_INT,          0,      500000  },
    {   'I',   "--test-int-cfg",  CMD_INT_CFG,      0,      500000  },
    {   's',   "--test-string",   CMD_STRING,       0,      0       },
    {   'S',   "--test-strlist",  CMD_STRLIST,      0,      0       },
    {   'f',   "--test-float",    CMD_FLOAT,        0,      4.5     },
    {   'b',   "--test-bool",     CMD_BOOL,         0,      1       },
    {   'c',   "--test-count",    CMD_COUNT,        0,      8       },
    {   'v',   "--verbose",       CMD_COUNT,        0,      8       },
    {   ' ',   "--a",             CMD_STRING,       0,      8       },
    {   ' ',   "--b",             CMD_STRING,       0,      8       },
    {   ' ',   "--c",             CMD_STRING,       0,      8       },
    {   ' ',   "--d",             CMD_STRING,       0,      8       },
    {   ' ',   "--e",             CMD_STRING,       0,      8       },
    {   ' ',   "--f",             CMD_STRING,       0,      8       },
    {   ' ',   "--g",             CMD_STRING,       0,      8       },
    {   ' ',   "--h",             CMD_STRING,       0,      8       },
    {   0,     NULL,              CMD_NONE,         0,      0       }
  };
    
  cmdline = cmdline_parse (argsetup, argc, argv, 32);
  if (cmdline) {
    printf ("command line parsed successfully.\n");
  } else {
    printf ("error parsing command line\n");
    exit (1);
  }
  cmdline_inspect (cmdline);
  envnum = cmdline_parse_env (cmdline, argsetup, (char *) "CMDLINE_TEST", FALSE);
  printf ("%d arguments parsed from environment variable.\n", envnum);
  cmdline_inspect (cmdline);
  filenum = cmdline_parse_file (cmdline, argsetup, (char *) "cmdline.conf", FALSE);
  printf ("%d arguments parsed from file.\n", filenum);
  cmdline_inspect (cmdline);
  
  printf ("main: calling cmdline_print_errors:\n\n");
  cmdline_print_errors (cmdline, stdout, TRUE, TRUE);
  
  opt = cmdline_find_opt (cmdline, 'v', (char *) "verbose");
  if (opt) {
    printf ("found arg verbose (v)\n");
    cmdline_opt_inspect (opt);
  } else {
    printf ("opt verbose (v) was not found\n");
  }
  
  printf ("inspecting opts link list:\n");
  linklist_inspect (cmdline->opts);
  printf ("inspecting extra_args link list:\n");
  linklist_inspect (cmdline->extra_args);
  
  
  printf ("main: checkpoint 1\n");
  opt = cmdline_get_count (cmdline, ' ', (char *) "--test-count", &c);
  printf ("cmdline_option returned %ld, test count = %d\n", (long int) opt, i);
    
  printf ("main: checkpoint 2\n");
  opt = cmdline_get_count (cmdline, 'v', (char *) "--verbose", &v);
  printf ("cmdline_option returned %ld, verbose = %d\n", (long int) opt, i);
  
  printf ("main: checkpoint 3\n");
  opt = cmdline_get_string (cmdline, 's', (char *) "--test-string", test_str, 15);
  printf ("cmdline_get_string returned %ld, test_str=%s\n", (long int) opt, test_str);

  printf ("main: checkpoint 4\n");
  opt = cmdline_get_int (cmdline, 'i', (char *) "--test-int", &test_int);
  printf ("cmdline_get_int returned %ld, test_int=%d\n", (long int) opt, test_int);

  printf ("main: checkpoint 5\n");
  opt = cmdline_get_float (cmdline, 'f', (char *) "--test-float", &test_float);
  printf ("cmdline_get_float returned %ld, test_float=%f\n", (long int) opt, test_float);
  
  printf ("main: checkpoint 6\n");
  list = linklist_init (TRUE);
  i = cmdline_get_strlist (cmdline, 'S', (char *) "--test-strlist", list);
  printf ("cmdline_get_strlist returned %d, list contains:\n", i);
  item = list->first;
  while (item) {
    printf ("  %s\n", (char *) item->data);
    item = item->next;
  }
  linklist_dealloc (list);
  
  printf ("\nwriting output config file\n");
  i = cmdline_write_file (cmdline, (char *) "cmdline.cfg", TRUE);
  printf ("done, cmdline_write_file returned %d\n", i);
  
  printf ("\nmain: deallocating structure...\n");
  cmdline_dealloc (cmdline);
  printf ("main: end\n");
  return 0;
}

#else

int cmdline_test (int argc, char **argv) {
  // don't do anything
  return 0;
}

#endif

/*#ifdef __cplusplus
}
#endif*/



#endif // CMDLINE_IMPLEMENTATION

