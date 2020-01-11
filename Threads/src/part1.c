#include "lott.h"
#include "helper.h"

static void* map(void*);
static void* reduce(void*);

void initializeWebsite(website** web, char* filename);

static website* first_website = NULL; // IN THIS PART, WE WILL USE A GLOBAL HEAD OF A LINKED LIST TO STORE ALL THE INFO

int part1(){

    DIR *dir;
    struct dirent *dirp;
    dir = opendir(DATA_DIR);
    pthread_t tid;
    int tnum = 0;
    // THIS WHILE LOOP OPENS DIRECTORY, AND READS THROUGH ALL THE FILES, CREATING A WEBSITE STRUCT FOR EACH FILE
    while ((dirp=readdir(dir)) != NULL) {
        if (dirp->d_type == DT_REG && (strcmp(dirp->d_name, ".") || strcmp(dirp->d_name, ".."))){ // Ignoring the current and parent directories
            website* site = malloc(sizeof(website));
            initializeWebsite(&site, dirp->d_name);
            addWebsite(&site);
            pthread_create(&tid, NULL, map, (void*)site);
            char buf[32];
            sprintf(buf, "map: %d", tnum);
            pthread_setname_np(tid, buf);
            site->tid = tid;
            tnum++;
        }
    }
    // JOINS ALL THE MAP THREADS
    website* temp = first_website;
    while(temp != NULL) {
        pthread_join(temp->tid, NULL);
        temp = temp->next;
    }
    closedir(dir);
    reduce(first_website);
    recursiveFree(first_website);

    return 0;
}

static void* map(void* v){
    website* curr_site = (website*)v;
    FILE* fp;
    char path[80];
    strcpy(path, DATA_DIR);
    strcat(path, "/");
    strcat(path, curr_site->name);
    fp = fopen(path, "r");

    char unix_time[256];
    char ip[256];
    char duration_string[256];
    char country[256];
    int n;
    // For duration time
    float total_duration = 0;
    int user_count = 0;
    // For average # of users per year
    struct tm tl;
    float total_users = 0;
    int year_count = 0;
    int years[138] = {0};
    // For country count
    char* countries[10] = {NULL};
    int user_count_country[10] = {0};
    // FSCANF READS THE NEXT LINE, AND PARSE THE ARGUMENTS INTO CORRESPONDING VARIABLES
    while (fscanf(fp, FORMAT "," FORMAT "," FORMAT "," FORMAT " %n", unix_time, ip, duration_string, country, &n) != EOF) {
        if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
            float duration = (float)atof(duration_string);
            total_duration += duration;
            user_count++;
        } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
            time_t int_time = (time_t)atoi(unix_time);
            localtime_r(&int_time, &tl);
            char outstr[16];
            strftime(outstr, sizeof(outstr), "%Y", &tl);
            int year = atoi(outstr);
            int index = year - 1901;
            years[index]++;
        }  else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
            // countries is an array of 10. getCountryIndex returns the index of the country, if not present,
            // will return the index of the next available position to store the new country in. If present,
            // increment the counter at that returned index
            int country_index = getCountryIndex(countries, country);
            if(countryExists(countries, country) == false) {
                countries[country_index] = malloc(3);
                strcpy(countries[country_index],country);
                user_count_country[country_index]++;
            } else {
                user_count_country[country_index]++;
            }
        }
    }
    // In this part, I store all the results of each website into a struct
    if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
        curr_site->avg_duration = total_duration / user_count;
    } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
        for(int i = 0; i < 138; i++) {
            if(years[i] > 0) {
                total_users += years[i];
                year_count++;
            }
        }
        curr_site->avg_user_per_year = (float)total_users / (float)year_count;
    } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
        char country[3];
        strcpy(country, countries[0]);
        int mode = user_count_country[0];
        for(int i = 1; i < 10; i++) {
            if(countries[i] != NULL) {
                if(user_count_country[i] > mode) {
                    mode = user_count_country[i];
                    strcpy(country, countries[i]);
                } else if(user_count_country[i] == mode) {
                    if(strcmp(country, countries[i]) > 0)
                        strcpy(country, countries[i]);
                }
            }
        }
        for(int i = 9; i >= 0; i--) { //attempting to free
            if(countries[i] != NULL)
                free(countries[i]);
        }
        strcpy(curr_site->country, country);
        curr_site->country_count = mode;
    }
    fclose(fp);
    return (void*)curr_site; // Didn't really use the return value.
}

static void* reduce(void* v){
    // In this part, I traverse through the linked list, with the global head first_website, and perform all the calculations
    if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
        if(!strcmp(QUERY_STRINGS[current_query], "A")) {
            float max = first_website->avg_duration;
            website* maxWebsite = first_website;
            website* temp = first_website;
            while(temp->next != NULL) {
                if(temp->next->avg_duration > max) {
                    max = temp->next->avg_duration;
                    maxWebsite = temp->next;
                } else if(temp->next->avg_duration == max) {
                    if(strcmp(maxWebsite->name, temp->next->name) > 0)
                        maxWebsite = temp->next;
                }
                temp = temp->next;
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], max, maxWebsite->name);
        } else {
            float min = first_website->avg_duration;
            website* minWebsite = first_website;
            website* temp = first_website;
            while(temp->next != NULL) {
                if(temp->next->avg_duration < min) {
                    min = temp->next->avg_duration;
                    minWebsite = temp->next;
                } else if(temp->next->avg_user_per_year == min) {
                    if(strcmp(minWebsite->name, temp->next->name) > 0)
                        minWebsite = temp->next;
                }
                temp = temp->next;
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], min, minWebsite->name);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
        if(!strcmp(QUERY_STRINGS[current_query], "C")) {
            float max = first_website->avg_user_per_year;
            website* maxWebsite = first_website;
            website* temp = first_website;
            while(temp->next != NULL) {
                if(temp->next->avg_user_per_year > max) {
                    max = temp->next->avg_user_per_year;
                    maxWebsite = temp->next;
                } else if(temp->next->avg_user_per_year == max) {
                    if(strcmp(maxWebsite->name, temp->next->name) > 0)
                        maxWebsite = temp->next;
                }
                temp = temp->next;
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], max, maxWebsite->name);
        } else {
            float min = first_website->avg_user_per_year;
            website* minWebsite = first_website;
            website* temp = first_website;
            while(temp->next != NULL) {
                if(temp->next->avg_user_per_year < min) {
                    min = temp->next->avg_user_per_year;
                    minWebsite = temp->next;
                } else if(temp->next->avg_user_per_year == min) {
                    if(strcmp(minWebsite->name, temp->next->name) > 0)
                        minWebsite = temp->next;
                }
                temp = temp->next;
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], min, minWebsite->name);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
        char* countries[10] = {NULL};
        int total_count_per_country[10] = {0};
        website* temp = first_website;
        // CREATES A NEW ARRAY. STORES NEW COUNTRIES INTO ARRAY OR INCREMENT.
        while(temp != NULL) {
            int index = getCountryIndex(countries, temp->country);
            if(countryExists(countries, temp->country) == false) {
                countries[index] = temp->country;
                total_count_per_country[index] += temp->country_count;
            } else {
                total_count_per_country[index] += temp->country_count;
            }

            temp = temp->next;
        }
        // FINDS THE COUNTRY WITH THE MOST USERS ALPHATETICALLY
        int most_users = total_count_per_country[0];
        char country[3];
        strcpy(country, countries[0]);
        for(int i = 1; i < 10; i++) {
            if(countries[i] != NULL) {
                if(total_count_per_country[i] > most_users) {
                    most_users = total_count_per_country[i];
                    strcpy(country, countries[i]);
                } else if(total_count_per_country[i] == most_users) {
                    if(strcmp(country, countries[i]) > 0)
                        strcpy(country, countries[i]);
                }
            }
        }
        printf("Part: %s\nQuery: %s\nResult: %d, %s\n",
            PART_STRINGS[current_part], QUERY_STRINGS[current_query], most_users, country);
    }
    return NULL;
}

// Puts website into website list
static void addWebsite(website** sitep) { 
    website* site = *sitep;
    if(first_website == NULL)
        first_website = site;
    else if(first_website->next == NULL)
        first_website->next = site;
    else {
        website* temp;
        temp = first_website;
        while(temp != NULL) {
            if(temp->next == NULL) {
                temp->next = site;
                break;
            }
            else
                temp = temp->next;
        }
    }
}

// Return true if the country exists, else false
static bool countryExists(char* countries[10], char* country) {
    for(int i = 0; i < 10; i++) {
        if(countries[i] == NULL) 
            return false;
        if(!strcmp(countries[i], country))
            return true;
    }
    return false;
}

// Returns the index of the given country, or the index of next empty spot
static int getCountryIndex(char* countries[10], char* country) {
    int index = 0;
    for(int i = 0; i < 10; i++) {
        if(countries[i] == NULL) {
            index = i;
            return index;
        }
        if(!strcmp(countries[i], country)){
            index = i;
            return index;
        }
    }
    return -1;
}

// Initialize the website struct
void initializeWebsite(website** web, char* filename) {
    website* site = *web;
    site->next = NULL;
    strcpy(site->name, filename);
}

// A recursive method that I use to free the linked list of website struct
static void recursiveFree(website* web) {
    if(web == NULL) {
        return;
    }
    recursiveFree(web->next);
    free(web);
}