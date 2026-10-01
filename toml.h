#ifndef TOML_H
#define TOML_H
#ifdef _MSC_VER
#	pragma warning(disable : 4996)
#endif
#ifdef __cplusplus
#	define TOML_EXTERN extern "C"
#else
#	define TOML_EXTERN extern
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Optional subsets, off by default so the parser keeps its upstream shape.
 *
 * TOML_NO_FLOAT removes double support. Parsing a double needs strtod and the
 * floating point registers, which a freestanding driver built with
 * -mgeneral-regs-only may not touch.
 *
 * TOML_NO_TIMESTAMP removes date and time support, which the caller has no
 * use for.
 *
 * TOML_NO_FILE removes toml_parse_file(), for callers that feed the parser a
 * buffer they read themselves. It drops the FILE and buffered I/O dependency.
 *
 * Defining any of them keeps the matching values out of the returned union
 * and drops the matching accessors; the parser then reports those values as
 * unknown rather than accepting them. */

typedef struct toml_table_t     toml_table_t;
typedef struct toml_array_t     toml_array_t;
typedef struct toml_value_t     toml_value_t;
#ifndef TOML_NO_TIMESTAMP
typedef struct toml_timestamp_t toml_timestamp_t;
#endif
typedef struct toml_keyval_t    toml_keyval_t;
typedef struct toml_arritem_t   toml_arritem_t;
typedef struct toml_pos_t       toml_pos_t;

// TOML table.
struct toml_table_t {
	const char* key;      // Key for this table
	int         keylen;   // length of key.
	bool        implicit; // Table was created implicitly
	bool        readonly; // No more modification allowed

	int             nkval; // key-values in the table
	toml_keyval_t** kval;
	int             narr; // arrays in the table
	toml_array_t**  arr;
	int             ntbl; // tables in the table
	toml_table_t**  tbl;
};

// TOML array.
struct toml_array_t {
	const char*     key;    // key to this array
	int             keylen; // length of key.
	int             kind;   // element kind: 'v'alue, 'a'rray, or 't'able, 'm'ixed
	int             type;   // for value kind: 'i'nt, 'd'ouble, 'b'ool, 's'tring, 't'ime, 'D'ate, 'T'imestamp, 'm'ixed
	int             nitem;  // number of elements
	toml_arritem_t* item;
};
struct toml_arritem_t {
	int           valtype; // for value kind: 'i'nt, 'd'ouble, 'b'ool, 's'tring, 't'ime, 'D'ate, 'T'imestamp
	char*         val;
	toml_array_t* arr;
	toml_table_t* tbl;
};

// TOML key/value pair.
struct toml_keyval_t {
	const char* key;    // key to this value
	int         keylen; // length of key.
	const char* val;    // the raw value
};

// Token position.
struct toml_pos_t {
	int line;
	int col;
};

// Timestamp type; some values may be empty depending on the value of kind.
// Unavailable when TOML_NO_TIMESTAMP is defined.
#ifndef TOML_NO_TIMESTAMP
struct toml_timestamp_t {
	// datetime type:
	//
	//   'd'atetime          Full date + time + TZ
	//   'l'local-datetime   Full date + time but without TZ
	//   'D'ate-local        Date only, without TZ
	//   't'ime-local        Time only, without TZ
	char kind;

	int year, month, day;
	int hour, minute, second, millisec;
	int tz; // Timezone offset in minutes
};
#endif

// Parsed TOML value.
//
// The string value s is a regular NULL-terminated C string, but the string
// length is also given in sl since TOML values may contain NULL bytes. The
// value is guaranteed to be correct UTF-8.
struct toml_value_t {
	bool ok; // Was this value present?
	union {
		struct {
			char* s;  // string value; must be freed after use.
			int   sl; // string length, excluding NULL.
		};
#ifndef TOML_NO_TIMESTAMP
		toml_timestamp_t ts; // datetime
#endif
		bool             b;  // bool
		int64_t          i;  // int
#ifndef TOML_NO_FLOAT
		double           d;  // double
#endif
	} u;
};

// toml_parse() parses a TOML document from a string. Returns 0 on error, with
// the error message stored in errbuf.
//
// toml_parse() modifies the buffer in place and keeps pointers into it, so the
// buffer must stay alive for as long as the returned table.
//
// Use toml_free() to free the return value; this will invalidate all handles
// for this table.
TOML_EXTERN toml_table_t* toml_parse(char* toml, char* errbuf, int errbufsz);
#ifndef TOML_NO_FILE
// toml_parse_file() is identical, but reads from a file descriptor.
TOML_EXTERN toml_table_t* toml_parse_file(FILE* fp, char* errbuf, int errbufsz);
#endif
TOML_EXTERN void          toml_free(toml_table_t* table);

// Table functions.
//
// toml_table_len() gets the number of direct keys for this table;
// toml_table_key() gets the nth direct key in this table.
TOML_EXTERN int           toml_table_len(const toml_table_t* table);
TOML_EXTERN const char*   toml_table_key(const toml_table_t* table, int keyidx, int* keylen);

// Returns the raw, unparsed text of a key, or NULL when it is absent. The
// pointer refers into the parsed document and stays valid until toml_free.
// This is the presence test: it is non-NULL for every kind of value, including
// the ones the typed accessors below cannot represent.
TOML_EXTERN const char*   toml_table_unparsed(const toml_table_t* table, const char* key);
TOML_EXTERN toml_value_t  toml_table_string(const toml_table_t* table, const char* key);

// Like toml_table_string, but returns a pointer into the parsed document
// instead of an allocated copy, so nothing has to be freed. Only the simple
// quoted forms are accepted, because they are all a configuration file needs:
// escapes, multiline strings and literal quotes inside the value all yield
// NULL. On success *len receives the content length, excluding the quotes.
//
// The value is terminated in place by overwriting its closing quote, so the
// result is an ordinary C string. That overwrite is why this may be called
// only once per key.
TOML_EXTERN const char*   toml_table_string_ref(const toml_table_t* table, const char* key, int* len);
TOML_EXTERN toml_value_t  toml_table_bool(const toml_table_t* table, const char* key);
TOML_EXTERN toml_value_t  toml_table_int(const toml_table_t* table, const char* key);
#ifndef TOML_NO_FLOAT
TOML_EXTERN toml_value_t  toml_table_double(const toml_table_t* table, const char* key);
#endif
#ifndef TOML_NO_TIMESTAMP
TOML_EXTERN toml_value_t  toml_table_timestamp(const toml_table_t* table, const char* key);
#endif
TOML_EXTERN toml_array_t* toml_table_array(const toml_table_t* table, const char* key);
TOML_EXTERN toml_table_t* toml_table_table(const toml_table_t* table, const char* key);

// Array functions.
TOML_EXTERN int           toml_array_len(const toml_array_t* array);
TOML_EXTERN toml_value_t  toml_array_string(const toml_array_t* array, int idx);
TOML_EXTERN toml_value_t  toml_array_bool(const toml_array_t* array, int idx);
TOML_EXTERN toml_value_t  toml_array_int(const toml_array_t* array, int idx);
#ifndef TOML_NO_FLOAT
TOML_EXTERN toml_value_t  toml_array_double(const toml_array_t* array, int idx);
#endif
#ifndef TOML_NO_TIMESTAMP
TOML_EXTERN toml_value_t  toml_array_timestamp(const toml_array_t* array, int idx);
#endif
TOML_EXTERN toml_array_t* toml_array_array(const toml_array_t* array, int idx);
TOML_EXTERN toml_table_t* toml_array_table(const toml_array_t* array, int idx);

#endif // TOML_H
