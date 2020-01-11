#include "lott.h"
#include "helper.h"

static void* map(void*);
static void* reduce(void*);
static void addChild(website** head, website** web);
static void initializeWebsite2(website** web, char* filename);

typedef struct {
    struct website** buf;
    int n;
    int front;
    int rear;
    int count;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t; 

void sbuf_init(sbuf_t *sp, int n); 
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, website* item);
website* sbuf_remove(sbuf_t *sp);


static website* first_website = NULL;
sbuf_t* global_buffer = NULL; // GLOBAL BUFFER, FIFO, ACTS LIKE A QUEUE.
static int flag = 1;

int part4(size_t nthreads){

    DIR *dir;
    struct dirent *dirp;
    dir = opendir(DATA_DIR);
    pthread_t tid;
    int tnum = 0;
    int fileNum = 0;

    global_buffer = malloc(sizeof(sbuf_t));
    sbuf_init(global_buffer, 50);

    website* threadsArray[nthreads];
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
    for(int i = 0; i < nthreads; i++) {
        if(i == fileNum) break;
        pthread_create(&tid, NULL, map, (void*)threadsArray[i]);
        char buf[32];
        sprintf(buf, "map: %d", tnum);
        pthread_setname_np(tid, buf);
        threadsArray[i]->tid = tid;
        tnum++;
    }
    pthread_create(&tid, NULL, reduce, (void*)global_buffer);
    pthread_setname_np(tid, "reduce");

    for(int i = 0; i < nthreads; i++) {
        if(i == fileNum) break;
        pthread_join(threadsArray[i]->tid, NULL);
    }
    flag = 0;
    pthread_join(tid, NULL);
    closedir(dir);
    sbuf_deinit(global_buffer);
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
        // countries[0] = NULL;
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
            temp->avg_duration = total_duration / user_count;
            sbuf_insert(global_buffer, temp);
        } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
            for(int i = 0; i < 138; i++) {
                if(years[i] > 0) {
                    total_users += years[i];
                    year_count++;
                }
            }
            temp->avg_user_per_year = (float)total_users / (float)year_count;
            sbuf_insert(global_buffer, temp);
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
            for(int i = 9; i >= 0; i--) { //attempting to free
                if(countries[i] != NULL)
                    free(countries[i]);
            }
            strcpy(temp->country, country);
            temp->country_count = mode;
            sbuf_insert(global_buffer, temp);
        }
        temp = temp->child;
        fclose(fp);
    }
    return (void*)website_head;
}

static void* reduce(void* v){
    float result;
    char resName[64];
    if(!strcmp(QUERY_STRINGS[current_query], "A") || !strcmp(QUERY_STRINGS[current_query], "B")) {
        if(!strcmp(QUERY_STRINGS[current_query], "A")) {
            result = 0;
            while(flag) {
                if(global_buffer->count > 0) {
                    website* web = sbuf_remove(global_buffer);
                    float avg_duration = web->avg_duration;
                    if(avg_duration > result) {
                        result = avg_duration;
                        strcpy(resName, web->name);
                    } else if( avg_duration == result) {
                        if(strcmp(resName, web->name) > 0)
                            strcpy(resName, web->name);
                    }
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(flag) {
                if(global_buffer->count > 0) {
                    website* web = sbuf_remove(global_buffer);
                    float avg_duration = web->avg_duration;
                    if(avg_duration < result) {
                        result = avg_duration;
                        strcpy(resName, web->name);
                    } else if( avg_duration == result) {
                        if(strcmp(resName, web->name) > 0)
                            strcpy(resName, web->name);
                    }
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
        if(!strcmp(QUERY_STRINGS[current_query], "C")) {
            result = 0;
            while(flag) {
                if(global_buffer->count > 0) {
                    website* web = sbuf_remove(global_buffer);
                    float avg_user_per_year = web->avg_user_per_year;
                    if(avg_user_per_year > result) {
                        result = avg_user_per_year;
                        strcpy(resName, web->name);
                    } else if( avg_user_per_year == result) {
                        if(strcmp(resName, web->name) > 0)
                            strcpy(resName, web->name);
                    }
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(flag) {
                if(global_buffer->count > 0) {
                    website* web = sbuf_remove(global_buffer);
                    float avg_user_per_year = web->avg_user_per_year;
                    if(avg_user_per_year < result) {
                        result = avg_user_per_year;
                        strcpy(resName, web->name);
                    } else if( avg_user_per_year == result) {
                        if(strcmp(resName, web->name) > 0)
                            strcpy(resName, web->name);
                    }
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
        char* countries[10] = {NULL};
        int total_count_per_country[10] = {0};
        while(flag) {
            if(global_buffer->count > 0) {
                website* web = sbuf_remove(global_buffer);
                int country_count = web->country_count;
                int index = getCountryIndex(countries, web->country); //webName holds countryName
                if(countryExists(countries, web->country) == false) {
                    countries[index] = malloc(3);
                    strcpy(countries[index], web->country);
                    total_count_per_country[index] += country_count;
                } else {
                    total_count_per_country[index] += country_count;
                }
            }
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

// INITIALIZES THE GLOBAL BUFFER. HAS THREE SEMAPHORES GUARANTEE WRITER PRIORITY
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = calloc(n, sizeof(website));
    sp->n = n;
    sp->front = sp->rear = sp->count = 0;
    sem_init(&sp->mutex, 0, 1);
    sem_init(&sp->slots, 0, n);
    sem_init(&sp->items, 0, 0);
} 

// FREE THE GLOBAL BUFFER
void sbuf_deinit(sbuf_t *sp) {
    free(sp->buf);
    free(sp);
}

// INSERT ITEMS INTO THE BUFFER, USED BY MAP, # SLOTS DECREMENT, # ITEMS INCREMENT
void sbuf_insert(sbuf_t *sp, website* item) {
    sem_wait(&sp->slots);
    sem_wait(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    sp->count++;
    sem_post(&sp->mutex);
    sem_post(&sp->items);
}

// REMOVE ITEMS FROM THE BUFFER, USED BY REDUCE, # SLOTS INCREMENT, # ITEMS DECREMENT  
website* sbuf_remove(sbuf_t *sp) {
    website* item = NULL;
    sem_wait(&sp->items);
    sem_wait(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    sp->count--;
    sem_post(&sp->mutex);
    sem_post(&sp->slots);
    return item;
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