#include "lott.h"
#include "helper.h"

static void* map(void*);
static void* reduce(void*);
static void addChild(website** head, website** web);
static void initializeWebsite2(website** web, char* filename);

typedef struct {
    char resName[64];
    float result;
} results;

static website* first_website = NULL;
struct pollfd* fds;
int global_counter = 0;

sem_t mutex;


//NOTE: THIS PART REQUIRES A LARGE NUMBER OF FILE DESCRIPTORS TO WORK. ulimit -n WAS SET TO 10000
//THEN IT STARTED TO WORK.
int part5(size_t nthreads){

    DIR *dir;
    struct dirent *dirp;
    dir = opendir(DATA_DIR);
    pthread_t tid;
    pthread_t redtid;
    int tnum = 0;
    int fileNum = 0;

    website* threadsArray[nthreads];

    int mapSockets[nthreads];
    sem_init(&mutex, 0 , 1);

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
    if(fileNum < nthreads) {
        nthreads = fileNum;
    }
    fds = malloc(sizeof(struct pollfd)*nthreads);

    // FOR LOOP TO CREATE SOCKETPAIRS. PAIR[0] GOES INTO MAPSOCKETS, SOCKET ENDS FOR MAP
    // THE OTHER END GOES TO RED, WHICH IS STORE IN FDS, A POLLFD ARRAY 
    for(int i = 0; i < nthreads; i++) {
        int pair[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
        mapSockets[i] = pair[0];
        fds[i].fd = pair[1];
        fds[i].events = POLLIN | POLLHUP;
    }

    pthread_create(&tid, NULL, reduce, &nthreads);
    pthread_setname_np(tid, "reduce");
    redtid = tid;

    // CREATES THE THREADS. ARGUMENT IS A LINKED LIST HEAD STILL, FOR EVENLY DISTRIBUTED WORK
    // A SOCKETFD IS PASSED INTO IT, ONE SOCKETFD FOR EACH MAP THREAD
    for(int i = 0; i < nthreads; i++) {
        threadsArray[i]->socketfd = mapSockets[i];
        pthread_create(&tid, NULL, map, (void*)threadsArray[i]);
        char buf[32];
        sprintf(buf, "map: %d", tnum);
        pthread_setname_np(tid, buf);
        threadsArray[i]->tid = tid;
        tnum++;
    }

    for(int i = 0; i < nthreads; i++) {
        pthread_join(threadsArray[i]->tid, NULL);
    }

    pthread_join(redtid, NULL);

    closedir(dir);
    free(fds);
    recursiveFree(first_website);

    return 0;
}

static void* map(void* v){
    website* website_head = (website*)v;
    website* temp = website_head;
    int socketfd = website_head->socketfd;
    
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
        // char* ptr;
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
        char buf[64];
        memset(buf, '\0', 64);
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
            sprintf(buf, "%s %f", temp->name, temp->avg_duration);
            sem_wait(&mutex);
            write(socketfd, buf, 64);
            sem_post(&mutex);
        } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
            for(int i = 0; i < 138; i++) {
                if(years[i] > 0) {
                    total_users += years[i];
                    year_count++;
                }
            }
            temp->avg_user_per_year = (float)total_users / (float)year_count;
            sprintf(buf, "%s %f\n", temp->name, temp->avg_user_per_year);
            sem_wait(&mutex);
            write(socketfd, buf, 64);
            sem_post(&mutex);
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
            sprintf(buf, "%s %d\n", country, mode);
            sem_wait(&mutex);
            write(socketfd, buf, 64);
            sem_post(&mutex);
        }
        temp = temp->child;
        fclose(fp);
    }
    return (void*)website_head;
}

static void* reduce(void* v){
    int nfds = *(int*)v;
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
            // PUTS POLL IN A WHILE LOOP, WHILE THERE IS STUFF TO READ, KEEP READING.
            while(poll(fds, nfds, -1) > 0) {
                int rdfd = 0;
                // LOOKS FOR THE FD THAT WAS WRITTEN INTO AND READY TO BE READ
                for(int i = 0; i < nfds; i++) {
                    if(fds[i].revents & POLLIN) {
                        rdfd = fds[i].fd;
                        break;
                    }
                }
                if(rdfd) {
                    read(rdfd, string, 64);
                    strcpy(webName, strtok_r(string, " ", &ptr));
                    strcpy(outputString, strtok_r(NULL, " ", &ptr));
                    // count++;    
                    avg_duration = (float)atof(outputString);
                    // printf("Max is: %.5f\n", max);
                    if(avg_duration > result) {
                        result = avg_duration;
                        strcpy(resName, webName); 
                    } else if(avg_duration == result) {
                        if(strcmp(resName, webName) > 0)
                            strcpy(resName, webName);
                    }
                    // BREAK OUT OF THE LOOP CONDITION. IF # OF ITEMS READ = # OF FILES. SAME FOR OTHER QUERIES BELOW.
                    global_counter++;
                    if(global_counter == nfds)
                        break;
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(poll(fds, nfds, -1) > 0) {
                int rdfd = 0;
                for(int i = 0; i < nfds; i++) {
                    if(fds[i].revents & POLLIN) {
                        rdfd = fds[i].fd;
                        break;
                    }
                }
                if(rdfd) {
                    read(rdfd, string, 64);

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
                    global_counter++;
                    if(global_counter == nfds)
                        break;
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "C") || !strcmp(QUERY_STRINGS[current_query], "D")) {
        float avg_user_per_year;
        if(!strcmp(QUERY_STRINGS[current_query], "C")) {
            result = 0;
            while(poll(fds, nfds, -1) > 0) {
                int rdfd = 0;
                for(int i = 0; i < nfds; i++) {
                    if(fds[i].revents & POLLIN) {
                        rdfd = fds[i].fd;
                        break;
                    }
                }
                if(rdfd) {
                    read(rdfd, string, 64);
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
                    global_counter++;
                    if(global_counter == nfds)
                        break;
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        } else {
            result = INT_MAX;
            while(poll(fds, nfds, -1) > 0) {
                int rdfd = 0;
                for(int i = 0; i < nfds; i++) {
                    if(fds[i].revents & POLLIN) {
                        rdfd = fds[i].fd;
                        break;
                    }
                }
                if(rdfd) {
                    read(rdfd, string, 64);

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
                    global_counter++;
                    if(global_counter == nfds)
                        break;
                }
            }
            printf("Part: %s\nQuery: %s\nResult: %.5g, %s\n",
                PART_STRINGS[current_part], QUERY_STRINGS[current_query], result, resName);
        }
    } else if(!strcmp(QUERY_STRINGS[current_query], "E")) {
        char* countries[10] = {NULL};
        int total_count_per_country[10] = {0};
        int country_count;
        while(poll(fds, nfds, -1) > 0) {
            int rdfd = 0;
            for(int i = 0; i < nfds; i++) {
                if(fds[i].revents & POLLIN) {
                    rdfd = fds[i].fd;
                    break;
                }
            }
            if(rdfd) {
                read(rdfd, string, 64);
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
                global_counter++;
                if(global_counter == nfds)
                    break;
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