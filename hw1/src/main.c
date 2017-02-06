    #include "map_reduce.h"

//Space to store the results for analysis map
struct Analysis analysis_space[NFILES];
//Space to store the results for stats map
Stats stats_space[NFILES];

//Sample Map function action: Print file contents to stdout and returns the number bytes in the file.
int cat(FILE* f, void* res, char* filename) {
    char c;
    int n = 0;
    printf("%s\n", filename);
    while((c = fgetc(f)) != EOF) {
        printf("%c", c);
        n++;
    }
    printf("\n");
    return n;
}
//int map(char* dir, void* results, size_t size, int (*act)(FILE* f, void* res, char* fn))
int main(int argc, char** argv) {
    int validity = validateargs(argc, argv);
    if(validity == 0) {
    	return EXIT_SUCCESS;
    }
    if(validity == -1) {
    	return EXIT_FAILURE;
    }
    int num_files = 0;
    if(validity == 1) { //analysis is chosen
    	num_files = nfiles(argv[argc-1]);
    	if(num_files == 0) {
    	printf("No files present in the directory\n");
    	return EXIT_SUCCESS;
    	}
    	if(num_files == -1) {
    		return EXIT_FAILURE;
    	}
    	int bytes = map(argv[argc-1], analysis_space, sizeof(struct Analysis), analysis);
    	if(bytes == -1) {
    		return EXIT_FAILURE;
    	}
    	struct Analysis result = analysis_reduce(num_files, analysis_space);
    	analysis_print(result, bytes, 1);	
    }
    if(validity == 2) { //stats is chosen
    	num_files = nfiles(argv[argc-1]);
    	if(num_files == 0) {
    	printf("No files present in the directory\n");
    	return EXIT_SUCCESS;
    	}
    	if(num_files == -1) {
    		return EXIT_FAILURE;
    	}
    	int bytes = map(argv[argc-1], stats_space, sizeof(Stats), stats);
    	if(bytes == -1) {
    		return EXIT_FAILURE;
    	}
    	Stats result = stats_reduce(num_files, stats_space);
    	stats_print(result, 1);
    }
    if(validity == 3) { // -v analysis is chosen
    	num_files = nfiles(argv[argc-1]);
    	if(num_files == 0) {
    	printf("No files present in the directory\n");
    	return EXIT_SUCCESS;
    	}
    	if(num_files == -1) {
    		return EXIT_FAILURE;
    	}
    	int bytes = map(argv[argc-1], analysis_space, sizeof(struct Analysis), analysis);
    	if(bytes == -1) {
    		return EXIT_FAILURE;
    	}
    	int i;
    	for(i = 0; i < num_files; i++) {
    		analysis_print(analysis_space[i], bytes, 0);
    	}
    	struct Analysis result = analysis_reduce(num_files, analysis_space);
    	analysis_print(result, bytes, 1);	
    }
    if(validity == 4) { // -v stats is chosen
    	num_files = nfiles(argv[argc-1]);
    	if(num_files == 0) {
    	printf("No files present in the directory\n");
    	return EXIT_SUCCESS;
    	}
    	if(num_files == -1) {
    		return EXIT_FAILURE;
    	}
    	int bytes = map(argv[argc-1], stats_space, sizeof(Stats), stats);
    	if(bytes == -1) {
    		return EXIT_FAILURE;
    	}
    	int i;
    	for(i = 0; i < num_files; i++) {
    		stats_print(stats_space[i], 0);
    	}
    	Stats result = stats_reduce(num_files, stats_space);
    	stats_print(result, 1);
    }

    // map(argv[2], analysis_space, sizeof(struct Analysis), cat);
    // printf("%d", result);
    return EXIT_SUCCESS;
}




