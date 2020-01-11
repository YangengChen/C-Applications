//**DO NOT** CHANGE THE PROTOTYPES FOR THE FUNCTIONS GIVEN TO YOU. WE TEST EACH
//FUNCTION INDEPENDENTLY WITH OUR OWN MAIN PROGRAM.
#include "map_reduce.h"

//Implement map_reduce.h functions here.
void help() {
	printf("Usage: ./map_reduce [h|v] FUNC DIR\n");
	printf("\tFUNC\tWhich operation you would like to run on the data:\n");
	printf("\t    \tana - Analysis of various tet files in a directory.\n");
	printf("\t    \tstats - Calculates stats on ﬁles which contain only numbers.\n");
	printf("\tDIR \tThe directory in which the files are located.\n");
	printf("\n");
	printf("\tOptions:\n");
	printf("\t-h  \tPrints this help menu.\n");
	printf("\t-v  \tPrints the map function's results, stating the file it's from.\n");
}

int validateargs(int argc, char** argv) {
	if(argc < 2) {
		help();
		return -1;
	}
	if(strcmp("-h", argv[1]) == 0) {
		help();
		return 0;
	}
	if(argc > 4) {
		help();
		return -1;
	}
	if(strcmp("-v", argv[1]) == 0) {
		DIR* dir = opendir(argv[argc-1]);
		if(dir == NULL) {
			closedir(dir);
			help();
			return -1;
		}
		if(strcmp("ana", argv[2]) == 0) {
			if(dir == NULL){
				help();
				return -1;
			}
			closedir(dir);
			return 3;
		}
		else if(strcmp("stats", argv[2]) == 0) {
			if(dir == NULL){
				help();
				return -1;
			}
			closedir(dir);
			return 4;
		}
	}
	if(strcmp("ana", argv[1]) == 0) {
		DIR* dir = opendir(argv[argc-1]);
		if(dir == NULL) {
			help();
			return -1;
		}
		closedir(dir);
		return 1;
	}
	if(strcmp("stats", argv[1]) == 0) {
		DIR* dir = opendir(argv[argc-1]);
		if(dir == NULL) {
			help();
			return -1;
		}
		closedir(dir);
		return 2;
	}
	help();
	return -1;
}

int nfiles(char* dir) {
	int num_files = 0;
	DIR* dirp;
	struct dirent* entry;

	dirp = opendir(dir);
	if(dirp){
		while((entry = readdir(dirp)) != NULL) {
			if(entry->d_type == DT_REG) {
				num_files++;
			}
		}
	} else {
		closedir(dirp);
		help();
		return -1;	
	}
	closedir(dirp);
	return num_files;
}

int map(char* dir, void* results, size_t size, int (*act)(FILE* f, void* res, char* fn)) {
	//map(dir,results,size,functionname)
	DIR* d;
	struct dirent* entry;
	d = opendir(dir);
	char* pointer = memset(results, '\0',size*nfiles(dir));
	if(d) {
		int sum = 0;
		while((entry = readdir(d)) != NULL) {
			// printf("%d\n", entry->d_type);
			// printf("%d\n", DT_REG);
			if(entry->d_type == DT_REG) {
				// printf("inside");
				char dirp_path[256] = {0};
				char* file_name = entry->d_name;
				strcpy(dirp_path, dir);
				char file_name_copy[256] = {0};
				strcpy(file_name_copy, file_name);
				strcat(dirp_path, "/");
				char* file_path;
				// size_t file_length = strlen(file_name);
				// printf("%s\n", dirp_path);
				file_path = strcat(dirp_path, file_name_copy);
				// printf("%s\n", file_path); //  file name strncat might be wrong
				FILE* file;
				file = fopen(file_path, "r");
				if(file != NULL){
					int total = act(file, pointer, file_name);
					if(total == -1) {
						help();
						return -1;
					}
					pointer += size;
					sum += total;
				}
				else {
					help();
					return -1;
				}
				fclose(file);
			}
		}
		return sum;
	}
	else {
		return -1;
	}
}

// struct Analysis {
//   int ascii[128];  //space to store counts for each ASCII character.
//   int lnlen;       //the longest line’s length
//   int lnno;        //the longest line’s line number.
//   char* filename;  //the file corresponding to the struct.
// };

struct Analysis analysis_reduce(int n, void* results) {
	struct Analysis* pointer = (struct Analysis*)results;
	struct Analysis final = {{0}, 0, 0, ""};
	int longest_ll;
	int longest_ln;
	char* file_name;
	int i;
	for(i = 0; i < n; i++){
		int j;
		for(j = 0; j < 128; j++) {
			final.ascii[j] += pointer->ascii[j];
		}
		if(i-1 >= 0) {
			if(pointer->lnlen > (pointer - 1)->lnlen) {
				longest_ll = pointer->lnlen;
				longest_ln = pointer->lnno;
				file_name = pointer->filename;
			}
		}
		pointer++;
	} 
	final.lnlen = longest_ll;
	final.lnno = longest_ln;
	if(n == 1) {
		final.filename = (pointer-1)->filename;
	} else {
		final.filename = file_name;
	}
	return final;
}

Stats stats_reduce(int n, void* results) {
	Stats* pointer = (Stats*)results;
	int i;
	Stats final = {{0}, 0, 0, ""};
	for(i = 0; i < n; i++) {
		int j;
		for(j = 0; j < NVAL; j++) { 
			final.histogram[j] += pointer->histogram[j];
		}
		final.sum += pointer->sum;
		final.n += pointer->n;
		// final.filename = pointer-> filename;
		pointer++;
	}
	return final;
}

void analysis_print(struct Analysis res, int nbytes, int hist) {
	printf("File: %s\n", res.filename);
	if(res.lnlen == 0 && res.lnno == 0) {
		printf("Longest line length: %c\n", '-');
		printf("Longest line number: %c\n", '-');
	} else {
	printf("Longest line length: %d\n", res.lnlen);
	printf("Longest line number: %d\n", res.lnno);
	}
	if(hist == 0) {
		printf("\n");
	}
	if(hist != 0) {
		printf("Total Bytes in directory: %d\n", nbytes);
	}
	if(hist != 0) {
		printf("Histogram: \n");
		int i;
		for(i = 0; i < 128; i++) {
		 	if(res.ascii[i] != 0) {
		 		printf(" %d:", i);
		 		int k;
		 		for(k = 0; k < res.ascii[i]; k++) {
		 			printf("-");
		 		}
		 		printf("\n");
		 	}
		}
	}
}

void stats_print(Stats res, int hist) {
	int array[res.n];
	int index = 0;
	int freq = 0;
	int mode;
	if(hist != 0) {
		printf("Histogram: \n");
	} else {
		printf("File: %s\n", res.filename);
	}
	int i;
	for(i = 0; i < NVAL; i++) {
		if(res.histogram[i] != 0) {
			if(hist != 0){
				printf("%d  :", i);
			}
			if(res.histogram[i] > freq) {
				freq = res.histogram[i];
				mode = i;
			}
			int k;
			for(k = 0; k < res.histogram[i]; k++) {
				if(hist != 0) {
					printf("-");
				}
				array[index] = i;
				index++;
			}
			if(hist != 0) {
				printf("\n");
			}
		}		
	}
	if(hist != 0) {
		printf("\n");
	}
	// array[] holds an ordered list of all numbers, mean mode median Q1 Q3 Min Max
	double mean = (double) res.sum / res.n;
	printf("Count: %d\n", res.n);
	if(res.n == 0 && res.sum == 0) {
		printf("Mean: %c\n", '-');	
	}
	else {
		printf("Mean: %f\n", mean);
	}
	int*modes = (int*)malloc(NVAL*sizeof(int));
	// modes[0] = mode;
	int mode_count = 0;
	for(i = 0; i < NVAL; i++) {
		if(res.histogram[i] == res.histogram[mode]) {
			modes[mode_count] = i;
			mode_count++;
		}
	}
	printf("Mode: ");
	if(res.n == 0 && res.sum == 0) {
		printf("%c", '-');	
	} else {
		for(i = 0; i < mode_count; i++) {
			printf("%d ", modes[i]);
		}
	}
	free(modes);
	printf("\n");
	double median;
	int med_index = res.n / 2;
	int Q1_index = res.n / 4;
	int Q3_index = (res.n * 3) / 4;
	double Q1 = 0;
	double Q3 = 0;
	if(res.n % 2 == 0) {
	 	int med_sum = array[med_index - 1] + array[med_index];
	 	median = (double) med_sum / 2.0;
	 	if(res.n % 4 == 0) {
	 		Q1 = (double)((array[Q1_index] + array[Q1_index - 1]) / 2.0);
	 		Q3 = (double)((array[Q3_index] + array[Q3_index - 1]) / 2.0);
	 	}
	} else {
		median = array[med_index];
		Q1 = array[Q1_index];
		Q3 = array[Q3_index];
	}
	int min = array[0];
	int max = array[res.n - 1]; // res.n = index variable
	if(res.n == 0 && res.sum == 0) {
		printf("Median: %c\n", '-');
		printf("Q1: %c\n", '-');
		printf("Q3: %c\n", '-');
		printf("Min: %c\n", '-');
		printf("Max: %c\n\n", '-');
	} else {
		printf("Median: %f\n", median);
		printf("Q1: %f\n", Q1);
		printf("Q3: %f\n", Q3);
		printf("Min: %d\n", min);
		printf("Max: %d\n\n", max);
	}

}
// typedef struct Stats {
//   int histogram[NVAL]; //space to store the count for each number.
//   int sum;             //the sum total of all the numbers in the file.
//   int n;               //the total count of numbers the files.
//   char* filename;      //the file corresponding to the struct.
//                        //(don't print for final result)
// } Stats;
// void *memset(void *s, int c, size_t n);
// struct Analysis {
//   int ascii[128];  //space to store counts for each ASCII character.
//   int lnlen;       //the longest line’s length
//   int lnno;        //the longest line’s line number.
//   char* filename;  //the file corresponding to the struct.
// };
int analysis(FILE* f, void* res, char* filename) {
	int num_bytes = 0;
	int charCount = 0;
	int longestLineLength = 0;
	int longestLineNum = 1;
	int line_num = 1;
	int array[128] = {0};
	struct Analysis* pointer = (struct Analysis*) res;
	char character = '\0';
	while((character = fgetc(f)) != EOF) {
		num_bytes++;
		array[(int)character]++;
		charCount++;
		if(character == '\n') {
			if(charCount > longestLineLength){
				longestLineLength = charCount - 1; //-1 for the new line character
				longestLineNum = line_num;
			}
			line_num++;
			charCount = 0; //reset	
		}
	}
	if(num_bytes == 0) {
		longestLineNum = 0;
	}
	if(line_num == 1) {
		longestLineLength = charCount;
	}//void *memcpy(void *dest, const void *src, size_t n);
	*pointer = (struct Analysis){{0}, longestLineLength, longestLineNum, filename};
	memcpy(pointer->ascii, array, sizeof(int)*128);
	return num_bytes;
}

int stats(FILE* f, void* res, char* filename) {
	int array[NVAL] = {0};
	int total_sum = 0;
	int number_count = 0;
	int integer = 0;
	Stats* pointer = (Stats*) res;
	while((fscanf(f, "%d", &integer)) != EOF) { 
		if(integer < 0 || integer > (NVAL - 1)) {
			return -1;
		}
		total_sum += integer;
		array[integer]++;
		number_count++;
	}
	*pointer = (Stats){{0}, total_sum, number_count, filename};
	memcpy(pointer->histogram, array, NVAL*sizeof(int));
	return 0;
}


