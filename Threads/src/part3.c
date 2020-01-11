#include "lott.h"
#include "helper.h"

static void* map(void*);
static void* reduce(void*);
static void addChild(website** head, website** web);
static void initializeWebsite2(website** web, char* filename);

static website* first_website = NULL;
FILE* wr_fp, *rd_fp;
sem_t wr_mutex, rd_mutex;
static int flag = 1;

int part3(size_t nthreads){

    DIR *dir;
    struct dirent *dirp;
    dir = opendir(DATA_DIR);
    pthread_t tid;
    int tnum = 0;
    int fileNum = 0;

    wr_fp = fopen("mapred.tmp", "a");
    rd_fp = fopen("mapred.tmp", "r");

    sem_init(&wr_mutex, 0, 1);
    sem_init(&rd_mutex, 0, 1);

    website* threadsArray[nthreads]; // ARRAY OF SIZE nthreads THAT STORES THE HEAD OF A LINKED LIST
    // OPENS THE DIRECTORY, READ THROUGH EACH FILE. AS I READ, I KEEP COUNT OF NUMBER FILES. AND STORE THE FILES INTO AN ARRAY SEPARATELY.
    // EACH ELEMENT IN THE threadsArray ARRAY IS A HEAD OF A LINKED LIST, SO THAT IT EVENLY SPLITS THE WORK.  
    while ((dirp=readdir(dir)) != NULL) {
        if (dirp->d_type == DT_REG && (strcmp(dirp->d_name, ".") || strcmp(dirp->d_name, ".."))){ // Ignoring the current and parent directories
            website* site = malloc(sizeof(website));
            initializeWebsite2(&site, dirp->d_name);
            addWebsite(&site);
            int index = fileNum % nthreads;
            if(fileNum < nthreads) {
                threadsArray[index] = site;
            } else {
                addChild(&threadsArray[index], &site);
            }
            fileNum++;
        }
    }

    //CREATE MAP THREADS, IF THREADS > # OF FILES, BREAK, SO THAT # of THREADS = # of FILES
    for(int i = 0; i < nthreads; i++) {
        if(i == fileNum) break;
        pthread_create(&tid, NULL, map, (void*)threadsArray[i]);
        char buf[32];
        sprintf(buf, "map: %d", tnum);
        pthread_setname_np(tid, buf);
        threadsArray[i]->tid = tid;
        tnum++;
    }
    
    //CREATES REDUCE THREAD
    pthread_create(&tid, NULL, reduce, (void*)rd_fp);
    pthread_setname_np(tid, "reduce");

    for(int i = 0; i < nthreads; i++) {
        if(i == fileNum) break;
        pthread_join(threadsArray[i]->tid, NULL);
    }
    fclose(wr_fp);
    flag = 0;
    pthread_join(tid, NULL);

    fclose(rd_fp);
    closedir(dir);
    unlink("mapred.tmp");
    recursiveFree(first_website);

    return 0;
}

static void* map(void* v){
    website* website_head = (website*)v;
    website* temp = website_head;
    while(temp != NULL) {
        FILE* fp;
        char path[80];
        strcpy(path, DATA_DIR);
        strcat(path, "/");   
        strcat(path, temp->name);
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
        if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
            sem_wait(&wr_mutex);
            fprintf(wr_fp, "%s %f\n", temp->name, (total_duration/user_count));
            fflush(wr_fp);
            sem_post(&wr_mutex);
        } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
            for(int i = 0; i < 138; i++) {
                if(years[i] > 0) {
                    total_users += years[i];
                    year_count++;
                }
            }
            sem_wait(&wr_mutex);
            fprintf(wr_fp, "%s %f\n", temp->name, ((float)total_users / (float)year_count));
            fflush(wr_fp);
            sem_post(&wr_mutex);
        } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
            char country[3];
            strcpy(country, countries[0]);
            int mode = user_count_country[0];
            for(int i = 1; i < 10; i++) {
                if(user_count_country[i] > mode) {
                    mode = user_count_country[i];
                    strcpy(country, countries[i]);
                } else if(user_count_country[i] == mode) {
                    if(strcmp(country, countries[i]) > 0)
                        strcpy(country, countries[i]);
                }
            }
            sem_wait(&wr_mutex);
            fprintf(wr_fp, "%s %d\n", country, mode);
            fflush(wr_fp);
            sem_post(&wr_mutex);
        }
        temp = temp->child;
        fclose(fp);
    }
    return (void*)website_head;
}

static void* reduce(void* v){
    float result;
    char resName[64];
    char string[64];
    char outputString[64];
    char webName[64];
    char* ptr;
    if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
        float avg_duration;
        if(!strcmp(QUERY_STRINGS[current_query], "A")) {
            result = 0;
            while(flag) {
                usleep(20);
                sem_wait(&rd_mutex);
                while (fgets(string, 64, rd_fp) != NULL) {
                    strcpy(webName, strtok_r(string, " ", &ptr));
                    strcpy(outputString, strtok_r(NULL, " ", &ptr)); 
                    avg_duration = (float)atof(outputString);
                    if(avg_duration > result) {
                        result = avg_duration;
                        strcpy(resName, webName); 
                    } else if(avg_duration == result) {
                        if(strcmp(resName, webName) > 0)
                            strcpy(resName, webName);
                    }
                }
                sem_post(&rd_mutex);
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(flag) {
                usleep(20);
                sem_wait(&rd_mutex);
                while (fgets(string, 64, rd_fp) != NULL) {
                    strcpy(webName, strtok_r(string, " ", &ptr));
                    strcpy(outputString, strtok_r(NULL, " ", &ptr));
                    avg_duration = (float)atof(outputString);
                    if(avg_duration < result) {
                        result = avg_duration;
                        strcpy(resName, webName); 
                    } else if(avg_duration == result) {
                        if(strcmp(resName, webName) > 0)
                            strcpy(resName, webName);
                    }
                }
                sem_post(&rd_mutex);
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
        float avg_user_per_year;
        if(!strcmp(QUERY_STRINGS[current_query], "C")) {
            result = 0;
            while(flag) {
                usleep(20);
                sem_wait(&rd_mutex);
                while (fgets(string, 64, rd_fp) != NULL) {
                    strcpy(webName, strtok_r(string, " ", &ptr));
                    strcpy(outputString, strtok_r(NULL, " ", &ptr));
                    avg_user_per_year = (float)atof(outputString);
                    if(avg_user_per_year > result) {
                        result = avg_user_per_year;
                        strcpy(resName, webName); 
                    } else if(avg_user_per_year == result) {
                        if(strcmp(resName, webName) > 0)
                            strcpy(resName, webName);
                    }
                }
                sem_post(&rd_mutex);
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(flag) {
                usleep(20);
                sem_wait(&rd_mutex);
                while (fgets(string, 64, rd_fp) != NULL) {
                    strcpy(webName, strtok_r(string, " ", &ptr));
                    strcpy(outputString, strtok_r(NULL, " ", &ptr));
                    avg_user_per_year = (float)atof(outputString);
                    if(avg_user_per_year < result) {
                        result = avg_user_per_year;
                        strcpy(resName, webName); 
                    } else if(avg_user_per_year == result) {
                        if(strcmp(resName, webName) > 0)
                            strcpy(resName, webName);
                    }
                }
                sem_post(&rd_mutex);
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
        char* countries[10] = {NULL};
        int total_count_per_country[10] = {0};
        int country_count;
        while(flag) {
            usleep(20);
            sem_wait(&rd_mutex);
            while (fgets(string, 64, rd_fp) != NULL) {
                strcpy(webName, strtok_r(string, " ", &ptr)); //webName actually holds Country Name
                strcpy(outputString, strtok_r(NULL, " ", &ptr));
                country_count = atoi(outputString);
                int index = getCountryIndex(countries, webName); //webName holds countryName
                if(countryExists(countries, webName) == false) {
                    countries[index] = malloc(3);
                    strcpy(countries[index], webName);
                    total_count_per_country[index] += country_count;
                } else {
                    total_count_per_country[index] += country_count;
                }
            }
            sem_post(&rd_mutex);
        }
        result = total_count_per_country[0];
        strcpy(resName, countries[0]); //resName holds the countryName
        for(int i = 1; i < 10; i++) {
            if(total_count_per_country[i] > result) {
                result = total_count_per_country[i];
                strcpy(resName, countries[i]);
            } else if(total_count_per_country[i] == result) {
                if(strcmp(resName, countries[i]) > 0)
                    strcpy(resName, countries[i]);
            }
        }
        for(int i = 9; i >= 0; i--) { //attempting to free
            if(countries[i] != NULL)
                free(countries[i]);
        }
        printf("Part: %s\nQuery: %s\nResult: %d, %s\n",
            PART_STRINGS[current_part], QUERY_STRINGS[current_query], (int)result, resName);
    }
    return NULL;
}

static void addWebsite(website** sitep) { // Puts website into website list
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

static void addChild(website** web, website** web2) {
    website* head = *web;
    website* site = *web2;
    if(head->child == NULL)
        head->child = site;
    else {
        website* temp;
        temp = head;
        while(temp != NULL) {
            if(temp->child == NULL) {
                temp->child = site;
                break;
            }
            else
                temp = temp->child;
        }
    }
}

static bool countryExists(char* countries[10], char* country) {
    for(int i = 0; i < 10; i++) {
        if(countries[i] == NULL) 
            break;
        if(!strcmp(countries[i], country))
            return true;
    }
    return false;
}

static int getCountryIndex(char* countries[10], char* country) {
    int index = 0;
    for(int i = 0; i < 10; i++) {
        // printf("%d %s\n", i, countries[i]);
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

static void initializeWebsite2(website** web, char* filename) {
    website* site = *web;
    site->next = NULL;
    site->child = NULL;
    strcpy(site->name, filename);
}

static void recursiveFree(website* web) {
    if(web == NULL) {
        return;
    }
    recursiveFree(web->next);
    free(web);
}