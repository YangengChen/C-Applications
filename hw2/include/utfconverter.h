#ifndef utfconverter
#define utfconverter

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <inttypes.h>
#include <sys/times.h>
#include <limits.h>

#define MAX_BYTES 4
#define SURROGATE_SIZE 4
#define NON_SURROGATE_SIZE 2
#define NO_FD -1
#define OFFSET 2

#define FIRST  0
#define SECOND 1
#define THIRD  2
#define FOURTH 3

#ifdef __STDC__
#define P(x) x
#else
#define P(x) ()
#endif

/** The enum for endianness. */
typedef enum {UTF8, LITTLE, BIG} endianness;

/** The struct for a codepoint glyph. */
typedef struct {
	unsigned char bytes[MAX_BYTES];
	endianness end;
	bool surrogate;
} Glyph;

/** The given filename. */
char filename[257];
FILE* fp;
FILE* inputfp;
bool file_empty;
bool filenbom;
int writefd;
char namebuffer[PATH_MAX + 1];
char hostname[257];
char* res;
int vCount = 0;
char* input_encoding = NULL;
char* output_encoding = NULL;
int glyph_count = 0;
int ascii_count = 0;
int surrogate_count = 0;

struct stat infile;
struct stat outfile;
clock_t st_time;
clock_t en_time;
struct tms st_cpu;
struct tms en_cpu;

float read_realtime = 0;
float read_usertime = 0;
float read_systime = 0;

float convert_realtime = 0;
float convert_usertime = 0;
float convert_systime = 0;

float write_realtime = 0;
float write_usertime = 0;
float write_systime = 0;	

/** The usage statement. */
char* USAGE[] = { 
	"Command line utility for converting UTF files to and from UTF-8, UTF-16LE and UTF-16BE.\n\n",
	"Usage:\n\t./utf [-h|--help] -u OUT_ENC | --UTF=OUT_ENC IN_FILE [OUT_FILE]\n\n\t",
	"Option arguments: \n\t\t-h, --help\t\tDisplays this usage.\n\t\t-v, -vv\t\t\tToggles the verbosity of the program to level 1 or 2.\n\n\t",
	"Mandatory argument: \n\t\t-u OUT_ENC, --UTF=OUT_ENC\tSets the output encoding.\n\t\t\t\t\t\tValid values for OUT_ENC: 8, 16LE, 16BE\n\n\t",
	"Positional Arguments:\n\t\tIN_FILE\t\t\tThe file to convert.\n\t\t[OUT_FILE]\t\tOutput file name. If not present, defaults to stdout.\n"
};

/** Which endianness to convert to. */
endianness conversion;

/** Which endianness the source file is in. */
endianness source;

endianness output_end;
/**
 * A function that swaps the endianness of the bytes of an encoding from
 * LE to BE and vice versa.
 *
 * @param glyph The pointer to the glyph struct to swap.
 * @return Returns a pointer to the glyph that has been swapped.
 */
 Glyph* swap_endianness(Glyph* glyph);

/**
 * Fills in a glyph with the given data in data[2], with the given endianness 
 * by end.
 *
 * @param glyph 	The pointer to the glyph struct to fill in with bytes.
 * @param data[2]	The array of data to fill the glyph struct with.
 * @param end	   	The endianness enum of the glyph.
 * @param fd 		The int pointer to the file descriptor of the input 
 * 			file.
 * @return Returns a pointer to the filled-in glyph.
 */
 Glyph* fill_glyph(Glyph* glyph, unsigned char data[2], endianness end, int* fd);

/**
 * Writes the given glyph's contents to stdout.
 *
 * @param glyph The pointer to the glyph struct to write to stdout.
 */
 void write_glyph(Glyph* glpyh);

 void utf8_write_glyph(Glyph* glyph, int num);

/**
 * Calls getopt() and parses arguments.
 *
 * @param argc The number of arguments.
 * @param argv The arguments as an array of string.
 */
 void parse_args(int argc, char** argv);

/**
 * Prints the usage statement.
 */
 void print_help();

/**
 * Closes file descriptors and frees list and possibly does other
 * bookkeeping before exiting.
 *
 * @param The fd int of the file the program has opened. Can be given
 * the macro value NO_FD (-1) to signify that we have no open file
 * to close.
 */
 void quit_converter(int fd);

/* helper function */
 void verbosity_print(const char* filename);

/*helper times function */
 void start_clock(void);

 void end_clock(float* real, float* user, float* sys);

 Glyph* convert(Glyph* glyph, unsigned char data[4], endianness end, int* fd);

 Glyph* convert_16_to_8(Glyph* glyph, unsigned char data[4], endianness end, int* fd);

 int call_read(int* fd, unsigned char* buf, int num);

#endif