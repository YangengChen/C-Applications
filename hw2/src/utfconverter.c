#include "utfconverter.h"

int main(int argc, char** argv)
{	
	int fd;
	int outputfd;
	unsigned char buf[4] = {0};
	int rv;
	Glyph* glyph; 

	/*After calling parse_args(), filename and conversion should be set. */
	parse_args(argc, argv);
	inputfp = fopen(filename, "r");
	if(inputfp == NULL) {
		print_help();
		return EXIT_FAILURE;
	} else {
		fseek (inputfp, 0, SEEK_END);
		if(ftell(inputfp) == 0) {
			fclose(inputfp);
			print_help();
			return EXIT_FAILURE;
		}
		fclose(inputfp);
	}
	if(argv[optind+1] != NULL) {
		if(argv[optind+2] != NULL) {
			print_help();
			return EXIT_FAILURE;
		}
		stat(filename, &infile);
		stat(argv[optind+1], &outfile);
		if(infile.st_ino == outfile.st_ino) {
			print_help();
			return EXIT_FAILURE;
		} else {
			fp = fopen(argv[optind + 1], "a+");
			if (fp != NULL) {
				fseek (fp, 0, SEEK_END);
				if(ftell(fp) == 0) {
					file_empty = true;
				}
			}
			fclose(fp);
			if(file_empty) {
				writefd = open(argv[optind +1], O_APPEND | O_WRONLY);
			} else {
				outputfd = open(argv[optind+1], O_RDONLY);
				if (call_read(&outputfd, &buf[0], 2)  == 2) {
					if(buf[0] == 0xff && buf[1] == 0xfe) {
						output_end = LITTLE;
					} else if(buf[0] == 0xfe && buf[1] == 0xff) {
						output_end = BIG;
					} else if(buf[0] == 0xef && buf[1] == 0xbb) {
						if((call_read(&outputfd, &buf[2], 1) == 1) && buf[2] == 0xbf) {
							output_end = UTF8;
						}
					}
				}
				close(outputfd);
				if(conversion == output_end) {	
					writefd = open(argv[optind+1], O_APPEND | O_WRONLY);
					filenbom = true;
					if(output_end == LITTLE) {
						buf[0] = '\n';
						buf[1] = '\0';
						write(writefd, buf, 2);
					} else if(output_end == BIG ) {
						buf[0] = '\0';
						buf[1] = '\n';
						write(writefd, buf, 2);
					} else if(output_end == UTF8) {
						buf[0] = '\n';
						write(writefd, buf, 1);
					}
				} else {
					print_help();
					return EXIT_FAILURE;
				}
			}
		}
	} else {
		writefd = STDOUT_FILENO;
	}
	fd = open(filename, O_RDONLY);
	rv = 0; 

	glyph = (Glyph*)malloc(sizeof(Glyph)); 
	
	/* Handle BOM bytes for UTF16 specially. 
         * Read our values into the first and second elements. */
	if((rv = read(fd, &buf[0], 1)) == 1 && (rv = read(fd, &buf[1], 1)) == 1){ 	
		if(buf[0] == 0xff && buf[1] == 0xfe){
			source = LITTLE;
			input_encoding = "16LE";
		} else if(buf[0] == 0xfe && buf[1] == 0xff){
			source = BIG;
			input_encoding = "16BE";
		} else if((buf[0] == 0xef && buf[1] == 0xbb)) {
			if((read(fd, &buf[2], 1)) == 1 && buf[2] == 0xbf) {
				source = UTF8;
				input_encoding = "8";
			} else {
				free(glyph);
				print_help();
				return EXIT_FAILURE; 
			}
		} else {
			/*file has no BOM 
			free(glyph->bytes);*/
			free(glyph);
			print_help();
			return EXIT_FAILURE; 
		}
		memset(glyph, 0, sizeof(Glyph)); 

	}
	if(conversion == UTF8) {
		glyph->bytes[0] = 0xef;
		glyph->bytes[1] = 0xbb;
		glyph->bytes[2] = 0xbf;
		if(!filenbom) {
			utf8_write_glyph(glyph, 3);
		}
		while(call_read(&fd, &buf[0], 2) == 2) {
			glyph = fill_glyph(glyph, buf, source, &fd);
			glyph = convert_16_to_8(glyph, buf, source, &fd);
		} 
	} else if(source == UTF8) {
		if(conversion == LITTLE) {
			glyph->bytes[0] = 0xff;
			glyph->bytes[1] = 0xfe;
		} else if(conversion == BIG) {
			glyph->bytes[0] = 0xfe;
			glyph->bytes[1] = 0xff;
		}
		glyph->surrogate = false;
		glyph->end = conversion;
		if(!filenbom) {
			write_glyph(glyph);
		}
/**         Start reading rest of the bytes of the UTF-8 file     **/
		while((call_read(&fd, &buf[0], 1)) == 1) {
			glyph = convert(glyph, buf, conversion, &fd);
			write_glyph(glyph);
		}	
	} else {
		glyph = fill_glyph(glyph, buf, source, &fd);
		glyph = swap_endianness(glyph);
		if(!filenbom) {
			write_glyph(glyph);
		}
	/* Now deal with the rest of the bytes.*/
		while((rv = call_read(&fd, &buf[0], 1)) == 1 && (rv = call_read(&fd, &buf[1], 1)) == 1){
			write_glyph(swap_endianness(fill_glyph(glyph, buf, source, &fd)));	
		}
	}
	if(vCount > 0) {
		verbosity_print(filename);
	}
	free(glyph);
	quit_converter(fd);
	quit_converter(writefd);
	return 0;
}


Glyph* swap_endianness(Glyph* glyph) {
	unsigned char holder;
	start_clock();
	if(source == LITTLE && strcmp(input_encoding, output_encoding) != 0){
		holder = glyph->bytes[0];
		glyph->bytes[0] = glyph->bytes[1];
		glyph->bytes[1] = holder;
	if(glyph->surrogate && strcmp(input_encoding, output_encoding) != 0){  /* If a surrogate pair, swap the next two bytes. */
		holder = glyph->bytes[2];
		glyph->bytes[2] = glyph->bytes[3];
		glyph->bytes[3] = holder;
	}
	glyph->end = conversion;
}
end_clock(&convert_realtime, &convert_usertime, &convert_systime);
return glyph;
}

Glyph* fill_glyph(Glyph* glyph, unsigned char data[2], endianness end, int* fd) {
	unsigned int bits = 0; 
	start_clock();
	if(conversion != UTF8) {
		if(strcmp(input_encoding, output_encoding) == 0) {
			glyph->bytes[0] = data[0];
			glyph->bytes[1] = data[1];
		} else if(end == LITTLE) {
			glyph->bytes[0] = data[0];
			glyph->bytes[1] = data[1];
		} else if(end == BIG) {
			glyph->bytes[0] = data[1];
			glyph->bytes[1] = data[0];
		}
	}
	if(end == LITTLE) {
		bits |= (data[FIRST] + (data[SECOND] << 8));
	} else if(end == BIG) {
		bits |= ((data[FIRST] << 8) + data[SECOND]);
	}
	if(bits > 0 && bits < 128) {
		ascii_count++;
	}
	/* Check high surrogate pair using its special value range.*/
	if(bits > 0xD800 && bits < 0xDBFF){ 		
		if(call_read(fd, &data[FIRST], 1) == 1 && call_read(fd, &data[SECOND], 1) == 1){ 
			bits = 0;
			if(end == LITTLE) {
				bits |= (data[FIRST]  + (data[SECOND] << 8));
			} else if (end == BIG) {
				bits |= ((data[FIRST] << 8) + data[SECOND]);
			}
			if(bits > 0xDC00 && bits < 0xDFFF){ /* Check low surrogate pair.*/
			glyph->surrogate = true; 
			surrogate_count++;
		} else {
			lseek(*fd, -OFFSET, SEEK_CUR); 
			glyph->surrogate = false;
		}
	}
} else {
	glyph->surrogate = false;
}

if(!glyph->surrogate){
	glyph->bytes[THIRD] = glyph->bytes[FOURTH] = 0;
} else if(glyph->surrogate){
	if(conversion != UTF8){
		if(strcmp(input_encoding, output_encoding) == 0) {
			glyph->bytes[THIRD] = data[FIRST]; 
			glyph->bytes[FOURTH] = data[SECOND];
		} else if (end == LITTLE) {
			glyph->bytes[THIRD] = data[FIRST]; 
			glyph->bytes[FOURTH] = data[SECOND];
		} else if(end == BIG) {
			glyph->bytes[THIRD] = data[SECOND]; 
			glyph->bytes[FOURTH] = data[FIRST];
		} 
	} else {
		lseek(*fd, -4, SEEK_CUR);
		call_read(fd, &data[FIRST], 2);
	}
}
glyph->end = conversion;
end_clock(&convert_realtime, &convert_usertime, &convert_systime);
return glyph;
}

void utf8_write_glyph(Glyph* glyph, int num) {
	glyph_count++;
	start_clock();
	write(writefd, glyph->bytes, num);
	end_clock(&write_realtime, &write_usertime, &write_systime);
}

void write_glyph(Glyph* glyph) {
	glyph_count++;
	start_clock();
	if(glyph->surrogate){
		write(writefd, glyph->bytes, SURROGATE_SIZE);
	} else {
		write(writefd, glyph->bytes, NON_SURROGATE_SIZE);
	}
	end_clock(&write_realtime, &write_usertime, &write_systime);
}

void parse_args(int argc, char** argv) {
	int option_index, c, g;
	char* endian_convert = NULL;
	
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"UTF", required_argument, 0, 'u'},
		{0, 0, 0, 0}
	};

	/* If getopt() returns with a valid (its working correctly) 
	 * return code, then process the args! */
	while((c = getopt_long(argc, argv, "hu:v", long_options, &option_index)) != -1){
		switch(c){
			case 'h':
			print_help();
			exit(EXIT_SUCCESS);
			case 'u':
			endian_convert = optarg;
			output_encoding = endian_convert;
			break;
			case 'v':
			vCount++;
			break;
			default:
			print_help();
			exit(EXIT_FAILURE);
		}

	}
	for(g = 0; g < argc; g++) {
		if(strcmp("--UTF", argv[g]) == 0) {
			print_help();
			exit(EXIT_FAILURE);
		}
	}

	if(optind < argc){;
		filename[256] = '\0';
		strcpy(filename, argv[optind]);
	} else {
		print_help();
		exit(EXIT_FAILURE);
	}

	if(endian_convert == NULL){
		print_help();
		exit(EXIT_FAILURE);
	}

	if(strcmp(endian_convert, "16LE") == 0){ 
		conversion = LITTLE;
	} else if(strcmp(endian_convert, "16BE") == 0){
		conversion = BIG;
	} else if(strcmp(endian_convert, "8") == 0) {
		conversion = UTF8;
	} else {
		print_help();
		exit(EXIT_FAILURE);
	}
}

void print_help(void) {
	int i; 
	for(i = 0; i < 5; i++){
		printf("%s", USAGE[i]); 
	}
	quit_converter(NO_FD);
}

void quit_converter(int fd) {
	close(STDERR_FILENO);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	if(fd != NO_FD)
		close(fd);
	/*exit(0); */
	/* Ensure that the file is included regardless of where we start compiling from. */
}

void verbosity_print(const char* filename) {
	struct stat st;
	struct utsname unameData;

	hostname[256] = '\0';

	res = realpath(filename, namebuffer);

	stat(res, &st);

	fprintf(stderr, "Input file size: %.3fkb\n", ((float)st.st_size) / 1000);
	fprintf(stderr, "Input file path: %s\n", realpath(filename, namebuffer));

	gethostname(hostname, 256);

	fprintf(stderr, "Input file encoding: UTF-%s\n", input_encoding);
	fprintf(stderr, "Output encoding: UTF-%s\n", output_encoding);
	fprintf(stderr, "Hostmachine: %s\n", hostname);
	
	uname(&unameData);
	fprintf(stderr, "Operating System: %s\n", unameData.sysname);
	if(vCount > 1) {
		float ascii_percent = (float)ascii_count / (float)glyph_count;
		float surrogate_percent = (float)surrogate_count / (float)glyph_count;
		fprintf(stderr, "Reading: real=%f, user=%f, sys=%f\n", read_realtime, read_usertime, read_systime);
		fprintf(stderr, "Converting: real=%f, user=%f, sys=%f\n", convert_realtime, convert_usertime, convert_systime);
		fprintf(stderr, "Writing: real=%f, user=%f, sys=%f\n", write_realtime, write_usertime, write_systime);
		fprintf(stderr, "ASCII: %d%%\n", (int)((ascii_percent*100)+0.5));
		fprintf(stderr, "Surrogates: %d%%\n", (int)((surrogate_percent*100)+0.5));
		fprintf(stderr, "Glyphs: %d\n", glyph_count);
	}

}

int call_read(int* fd, unsigned char* buf, int num) {
	int bytes_read;
	start_clock();
	bytes_read = read(*fd, buf, num);
	end_clock(&read_realtime, &read_usertime, &read_systime);
	return bytes_read;
}

void start_clock(){
	st_time = times(&st_cpu);
}

void end_clock(float* real, float* user, float* sys) {
	en_time = times(&en_cpu);
	*real += (float)(en_time - st_time) / sysconf(_SC_CLK_TCK);
	*user += (float)(en_cpu.tms_utime - st_cpu.tms_utime) / sysconf(_SC_CLK_TCK);
	*sys  += (float)(en_cpu.tms_stime - st_cpu.tms_stime) / sysconf(_SC_CLK_TCK);
}

Glyph* convert(Glyph* glyph, unsigned char data[4], endianness conversion, int* fd) {
	unsigned int first_byte = 0;
	unsigned int second_byte = 0;
	unsigned int third_byte = 0;
	unsigned int fourth_byte = 0;
	unsigned int codepoint = 0;
	unsigned char bytes[4];
	unsigned int bits;
	unsigned int msbits;
	unsigned int lsbits;
	unsigned int surrogate_one;
	unsigned int surrogate_two;
	start_clock();
if(data[0] > 0 && data[0] < 128) { /* 1 byte */
	ascii_count++;
	if(conversion == LITTLE) {
		glyph->bytes[0] = data[0];
		glyph->bytes[1] = glyph->bytes[2] = glyph->bytes[3] = 0;
		glyph->end = conversion;
	} else if(conversion == BIG) {
		glyph->bytes[1] = data[0];
		glyph->bytes[0] = glyph->bytes[2] = glyph->bytes[3] = 0;
		glyph->end = conversion;
	}
	glyph->surrogate = false;
} else if(data[0] > 191 && data[0] < 224) { /* 2 bytes */
	if(call_read(fd, &data[1], 1) == 1) {
		first_byte = data[0];
		second_byte = data[1];
		first_byte &= 0x1f;
		second_byte &= 0x3f;
		first_byte = first_byte << 6;
		codepoint = first_byte | second_byte;
		bytes[2] = (codepoint >> 8) & 0xFF;
		bytes[3] = codepoint & 0xFF;
		if(conversion == LITTLE) {
			glyph->bytes[0] = bytes[3];
			glyph->bytes[1] = bytes[2];
			glyph->bytes[2] = glyph->bytes[3] = 0;
			glyph->end = conversion;
		} else if(conversion == BIG) {
			glyph->bytes[0] = bytes[2];
			glyph->bytes[1] = bytes[3];
			glyph->bytes[2] = glyph->bytes[3] = 0;
			glyph->end = conversion;
		}
		glyph->surrogate = false;
	}
} else if(data[0] > 223 && data[0] < 240) { /* 3 bytes */
	if(call_read(fd, &data[1], 2) == 2) {
		first_byte = data[0];
		second_byte = data[1];
		third_byte = data[2];
		first_byte &= 0xf;
		second_byte &= 0x3f;
		third_byte &= 0x3f;
		first_byte <<= 12;
		second_byte <<= 6;
		codepoint = first_byte | second_byte | third_byte;
		bytes[2] = (codepoint >> 8) & 0xFF;
		bytes[3] = codepoint & 0xFF;
		if(conversion == LITTLE) {
			glyph->bytes[0] = bytes[3];
			glyph->bytes[1] = bytes[2];
			glyph->bytes[2] = glyph->bytes[3] = 0;
			glyph->end = conversion;
		} else if(conversion == BIG) {
			glyph->bytes[0] = bytes[2];
			glyph->bytes[1] = bytes[3];
			glyph->bytes[2] = glyph->bytes[3] = 0;
			glyph->end = conversion;
		}
		glyph->surrogate = false;
	}
} else if(data[0] > 239 && data[0] < 248) { /* 4 bytes */
	if(call_read(fd, &data[1], 3) == 3) {
		first_byte = data[0];
		second_byte = data[1];
		third_byte = data[2];
		fourth_byte = data[3];
		first_byte  &= 0x7;
		second_byte &= 0x3f;
		third_byte &= 0x3f;
		fourth_byte &= 0x3f;
		first_byte <<= 18;
		second_byte <<= 12;
		third_byte <<= 6;
		codepoint = first_byte | second_byte | third_byte | fourth_byte;
			/*if code point is larger than 0xFFFF, needs to split into two 2-bytes */
		if(codepoint > 0xFFFF) {
			bits = codepoint - 0x10000;
			msbits = bits >> 10;
			lsbits = bits & 0x3ff;
			surrogate_one = msbits + 0xD800;
			surrogate_two = lsbits + 0xDC00;
			bytes[2] = (surrogate_one >> 8) & 0xFF;
			bytes[3] = surrogate_one & 0xFF; 
			if(conversion == LITTLE) {
				glyph->bytes[0] = bytes[3];
				glyph->bytes[1] = bytes[2];
				glyph->bytes[2] = surrogate_two & 0xFF;
				glyph->bytes[3] = (surrogate_two >> 8) & 0xFF;
				glyph->end = conversion;
			} else if(conversion == BIG) {
				glyph->bytes[0] = bytes[2];
				glyph->bytes[1] = bytes[3];
				glyph->bytes[2] = (surrogate_two >> 8) & 0xFF;
				glyph->bytes[3] = surrogate_two & 0xFF;
				glyph->end = conversion;
			}
			surrogate_count++;
			glyph->surrogate = true;
		} else {
			bytes[2] = (codepoint >> 8) & 0xFF;
			bytes[3] = codepoint & 0xFF; 
			if(conversion == LITTLE) {
				glyph->bytes[0] = bytes[3];
				glyph->bytes[1] = bytes[2];
				glyph->bytes[2] = glyph->bytes[3] = 0;
				glyph->end = conversion;
			} else if(conversion == BIG) {
				glyph->bytes[0] = bytes[2];
				glyph->bytes[1] = bytes[3];
				glyph->bytes[2] = glyph->bytes[3] = 0;
				glyph->end = conversion;
			}
			glyph->surrogate = false;
		}
	}
}	
return glyph;
}

Glyph* convert_16_to_8(Glyph* glyph, unsigned char data[4], endianness end, int* fd) {
	unsigned int bits = 0;
	unsigned int surrogate_high = 0;
	unsigned int surrogate_low = 0; 
	unsigned int first_byte;
	unsigned int second_byte;
	unsigned int third_byte;
	unsigned int fourth_byte;
	if(end == LITTLE) {
		if(!glyph->surrogate) {
			bits |= (data[0] + (data[1] << 8));
			if(bits > 0 && bits < 0x80) { /* ascii 1 byte*/
			glyph->bytes[0] = data[0];
			glyph->bytes[1] = glyph->bytes[2] = glyph->bytes[3] = 0;
			utf8_write_glyph(glyph, 1);
			} else if(bits > 0x7F && bits < 0x800) { /* 2 bytes 0x7C0 0x3F, add 0xC0, 0x80 */
			first_byte = bits & 0x7C0;
			first_byte >>= 6;
			first_byte += 0xC0;
			second_byte = bits & 0x3F;
			second_byte += 0x80;
			glyph->bytes[0] = first_byte;
			glyph->bytes[1] = second_byte;
			glyph->bytes[2] = glyph->bytes[3] = 0;
			utf8_write_glyph(glyph, 2);
			} else if(bits > 0x7FF && bits < 0x10000) { /* 3 bytes */
			first_byte = bits & 0xF000;
			first_byte >>= 12;
			first_byte += 0xE0;
			second_byte = bits & 0xFC0;
			second_byte >>= 6;
			second_byte += 0x80;
			third_byte = bits & 0x3F;
			third_byte += 0x80;
			glyph->bytes[0] = first_byte;
			glyph->bytes[1] = second_byte;
			glyph->bytes[2] = third_byte;
			glyph->bytes[3] = 0;
			utf8_write_glyph(glyph, 3);
		} 
		}else { /* 4 bytes surrogate */
		surrogate_high |= (data[0] + (data[1] << 8));
		if(call_read(fd, &data[2], 2) == 2) {
			surrogate_low |= (data[2] + (data[3] << 8));
		}
		surrogate_high -= 0xD800;
		surrogate_high <<= 10;
		 	surrogate_low -= 0xDC00; /* 10 LSBs */
		bits = surrogate_high + surrogate_low;
		bits += 0x10000;
		first_byte = bits & 0x1C0000;
		first_byte >>= 18;
		first_byte += 0xF0;
		second_byte = bits & 0x3F000;
		second_byte >>= 12;
		second_byte += 0x80;
		third_byte = bits & 0xFC0;
		third_byte >>= 6;
		third_byte += 0x80;
		fourth_byte = bits & 0x3F;
		fourth_byte += 0x80;
		glyph->bytes[0] = first_byte;
		glyph->bytes[1] = second_byte;
		glyph->bytes[2] = third_byte;
		glyph->bytes[3] = fourth_byte;
		utf8_write_glyph(glyph, 4);
	}
} else if(end == BIG) {
	if(!glyph->surrogate) {
		bits |= ((data[0] << 8) + data[1]);
			if(bits > 0 && bits < 0x80) { /* ascii 1 byte*/
		glyph->bytes[0] = data[1];
		glyph->bytes[1] = glyph->bytes[2] = glyph->bytes[3] = 0;
		utf8_write_glyph(glyph, 1);
			} else if(bits > 0x7F && bits < 0x800) { /* 2 bytes 0x7C0 0x3F, add 0xC0, 0x80 */
		first_byte = bits & 0x7C0;
		first_byte >>= 6;
		first_byte += 0xC0;
		second_byte = bits & 0x3F;
		second_byte += 0x80;
		glyph->bytes[0] = first_byte;
		glyph->bytes[1] = second_byte;
		glyph->bytes[2] = glyph->bytes[3] = 0;
		utf8_write_glyph(glyph, 2);
			} else if(bits > 0x7FF && bits < 0x10000) { /* 3 bytes */
		first_byte = bits & 0xF000;
		first_byte >>= 12;
		first_byte += 0xE0;
		second_byte = bits & 0xFC0;
		second_byte >>= 6;
		second_byte += 0x80;
		third_byte = bits & 0x3F;
		third_byte += 0x80;
		glyph->bytes[0] = first_byte;
		glyph->bytes[1] = second_byte;
		glyph->bytes[2] = third_byte;
		glyph->bytes[3] = 0;
		utf8_write_glyph(glyph, 3);
	} 
		} else { /* 4 bytes surrogate */
	surrogate_high |= ((data[0] << 8) + data[1]);
	if(call_read(fd, &data[2], 2) == 2) {
		surrogate_low |= ((data[2] << 8) + data[3]);
	}
	surrogate_high -= 0xD800;
	surrogate_high <<= 10;
		 	surrogate_low -= 0xDC00; /* 10 LSBs */
	bits = surrogate_high + surrogate_low;
	bits += 0x10000;
	first_byte = bits & 0x1C0000;
	first_byte >>= 18;
	first_byte += 0xF0;
	second_byte = bits & 0x3F000;
	second_byte >>= 12;
	second_byte += 0x80;
	third_byte = bits & 0xFC0;
	third_byte >>= 6;
	third_byte += 0x80;
	fourth_byte = bits & 0x3F;
	fourth_byte += 0x80;
	glyph->bytes[0] = first_byte;
	glyph->bytes[1] = second_byte;
	glyph->bytes[2] = third_byte;
	glyph->bytes[3] = fourth_byte;
	utf8_write_glyph(glyph, 4);
}
}
return glyph;
}