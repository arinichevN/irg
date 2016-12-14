/*
 * irg
 */


#include "main.h"

char pid_path[LINE_SIZE];
char app_class[NAME_SIZE];
char db_conninfo_settings[LINE_SIZE];

int app_state = APP_INIT;
PGconn *db_conn_settings = NULL;
PGconn *db_conn_public = NULL;
PGconn **db_connp_public = NULL;

PGconn *db_conn_data = NULL;
PGconn **db_connp_data = NULL; //pointer to db_conn_settings or to db_conn_data

int pid_file = -1;
int proc_id;
int udp_port = -1;
size_t udp_buf_size = 0;
int udp_fd = -1;
int udp_fd_tf = -1;
Lock lock;
Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
struct timespec rsens_interval_min = {1, 0};
struct timespec peer_ping_interval = {3, 0};
ThreadData thread_data_ctl = {.cmd = ACP_CMD_APP_NO, .on = 0, .created = 0, .attr_initialized = 0};
I1List i1l = {NULL, 0};
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};

PeerList peer_list = {NULL, 0};
EMList em_list = {NULL, 0};
SensorList sensor_list = {NULL, 0};
ValveList valve_list = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};


#include "util.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    char db_conninfo_data[LINE_SIZE];
    char db_conninfo_public[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);
    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        return 0;
    }
    snprintf(q, sizeof q, "select db_public from " APP_NAME_STR ".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        fputs("ERROR: readSettings: need only one tuple (1)\n", stderr);
        return 0;
    }


    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), LINE_SIZE);
    PQclear(r);
    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_connp_public = &db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            return 0;
        }
        db_connp_public = &db_conn_public;
    }

    udp_buf_size = 0;
    udp_port = -1;
    snprintf(q, sizeof q, "select udp_port, udp_buf_size, pid_path, db_data, cycle_duration_us, lock_key from " APP_NAME_STR ".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) == 1) {
        int done = 1;
        done = done && config_getUDPPort(*db_connp_public, PQgetvalue(r, 0, 0), &udp_port);
        done = done && config_getBufSize(*db_connp_public, PQgetvalue(r, 0, 1), &udp_buf_size);
        done = done && config_getPidPath(*db_connp_public, PQgetvalue(r, 0, 2), pid_path, LINE_SIZE);
        done = done && config_getDbConninfo(*db_connp_public, PQgetvalue(r, 0, 3), db_conninfo_data, LINE_SIZE);
        done = done && config_getCycleDurationUs(*db_connp_public, PQgetvalue(r, 0, 4), &cycle_duration);
        done = done && config_getLockKey(*db_connp_public, PQgetvalue(r, 0, 5), lock.key, LOCK_KEY_SIZE);
        if (!done) {
            PQclear(r);
            freeDB(&db_conn_public);
            fputs("ERROR: readSettings: failed to read some fields\n", stderr);
            return 0;
        }
    } else {
        PQclear(r);
        freeDB(&db_conn_public);
        fputs("ERROR: readSettings: one tuple expected\n", stderr);
        return 0;
    }
    PQclear(r);


    if (dbConninfoEq(db_conninfo_data, db_conninfo_settings)) {
        db_connp_data = &db_conn_settings;
    } else if (dbConninfoEq(db_conninfo_data, db_conninfo_public)) {
        db_connp_data = &db_conn_public;
    } else {
        if (!initDB(&db_conn_data, db_conninfo_data)) {
            return 0;
        }
        db_connp_data = &db_conn_data;
    }

    if (!(dbConninfoEq(db_conninfo_data, db_conninfo_settings) || dbConninfoEq(db_conninfo_public, db_conninfo_settings))) {
        freeDB(&db_conn_settings);
    }
    return 1;
}

int initLock(Lock *item, const EMList *el) {
    PGresult *r;
    char *q = "select em_id from " APP_NAME_STR ".lock_em";
    size_t i;
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        item->em_list[i] = NULL;
    }
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return 0;
    }
    int n;
    int n1 = PQntuples(r);
    if (n1 > LOCK_KEY_SIZE) {
        n = LOCK_KEY_SIZE;
    } else {
        n = n1;
    }
    if (n > 0) {
        for (i = 0; i < n; i++) {
            item->em_list[i] = getEMById(atoi(PQgetvalue(r, i, 0)), el);
        }
    }
    PQclear(r);
    if (!checkLock(item)) {
        item->key[0] = '\0';
    }
#ifdef MODE_DEBUG
    puts("initLock: done");
#endif
    return 1;
}

int checkLock(Lock *item) {
    if (item->key[0] == '\0') {
        return 0;
    }
    size_t i, n1 = 0, n2 = 0;
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        if (item->key[i] != '1' && item->key[i] != '0') {
            break;
        }
        n1++;
    }
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        if (item->em_list[i] == NULL) {
            break;
        }
        n2++;
    }
    if (n1 > n2) {
#ifdef MODE_DEBUG
        fputs("WARNING: checkLock: number_of_digits_in_key > number_of_EM\n", stderr);
#endif
        return 0;
    }
    if (n1 == 0) {
#ifdef MODE_DEBUG
        fputs("WARNING: checkLock: no key for lock\n", stderr);
#endif
        return 0;
    }
    if (n2 == 0) {
#ifdef MODE_DEBUG
        fputs("WARNING: checkLock: no EMs assigned to lock\n", stderr);
#endif
        return 0;
    }
    return 1;
}

void lockOpen(Lock *item) {
    if (item->key[0] == '\0') {
        return;
    }
    size_t i;
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        if (item->em_list[i] != NULL) {
            switch (item->key[i]) {
                case '1':
                    controlEM(item->em_list[i], TURN_ON);
                    break;
                case '0':
                    controlEM(item->em_list[i], TURN_OFF);
                    break;
                default:
                    return;
            }
        }
    }
}

void lockClose(Lock *item) {
    if (item->key[0] == '\0') {
        return;
    }
    size_t i;
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        if (item->em_list[i] != NULL) {
            switch (item->key[i]) {
                case '1':
                    controlEM(item->em_list[i], TURN_OFF);
                    break;
                case '0':
                    controlEM(item->em_list[i], TURN_ON);
                    break;
                default:
                    return;
            }
        }
    }
}

void initApp() {
    if (!readConf(CONF_FILE, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }

    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }

    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }

    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }

    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }

    if (!initUDPClient(&udp_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
    }
}

int initData() {
    char q[LINE_SIZE];
    printf("initData: db pointers:%p %p", *db_connp_data, *db_connp_public);
    snprintf(q, sizeof q, "select peer_id from " APP_NAME_STR ".em_mapping where app_class='%s' union distinct select peer_id from " APP_NAME_STR ".sensor_mapping where app_class='%s'", app_class, app_class);
    if (!config_getPeerList(*db_connp_data, *db_connp_public, q, &peer_list, &udp_fd_tf)) {
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    if (!initSensor(&sensor_list, &peer_list)) {
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    if (!initEM(&em_list, &peer_list)) {
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    if (!initLock(&lock, &em_list)) {
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    if (!initValve(&valve_list, &sensor_list, &em_list)) {
        FREE_LIST(&valve_list);
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    i1l.item = (int *) malloc(udp_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        FREE_LIST(&valve_list);
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }
    if (!createThread_ctl(&thread_data_ctl)) {
        FREE_LIST(&i1l);
        FREE_LIST(&valve_list);
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        FREE_LIST(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return 0;
    }

    return 1;
}

int initSensor(SensorList *list, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    list->length = 0;
    list->item = NULL;
    snprintf(q, sizeof q, "select sensor_id, remote_id, peer_id from " APP_NAME_STR ".sensor_mapping where app_class='%s'", app_class);
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return 0;
    }
    list->length = PQntuples(r);
    if (list->length > 0) {
        list->item = (Sensor *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
            fputs("ERROR: initSensor: failed to allocate memory\n", stderr);
            PQclear(r);
            return 0;
        }
        FORL{
            list->item[i].id = atoi(PQgetvalue(r, i, 0));
            list->item[i].remote_id = atoi(PQgetvalue(r, i, 1));
            list->item[i].source = getPeerById(PQgetvalue(r, i, 2), pl);
            if (!initMutex(&list->item[i].mutex)) {
                return 0;
            }
            clock_gettime(LIB_CLOCK, &list->item[i].last_read_time);
            list->item[i].last_return = 0;
        }
    }
    PQclear(r);
    if (!checkSensor(list)) {
        return 0;
    }
#ifdef MODE_DEBUG
    puts("initSensor: done");
#endif
    return 1;
}

int initEM(EMList *list, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    list->length = 0;
    list->item = NULL;
    snprintf(q, sizeof q, "select em_id, remote_id, peer_id from " APP_NAME_STR ".em_mapping where app_class='%s'", app_class);
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return 0;
    }
    list->length = PQntuples(r);
    if (list->length > 0) {
        list->item = (EM *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
            fputs("ERROR: initEm: failed to allocate memory\n", stderr);
            PQclear(r);
            return 0;
        }
        FORL{
            list->item[i].id = atoi(PQgetvalue(r, i, 0));
            list->item[i].remote_id = atoi(PQgetvalue(r, i, 1));
            list->item[i].source = getPeerById(PQgetvalue(r, i, 2), &peer_list);
            list->item[i].last_output = 0.0f;
            if (!initMutex(&list->item[i].mutex)) {
                return 0;
            }
        }

    }
    PQclear(r);

    if (!checkEM(list)) {
        return 0;
    }
#ifdef MODE_DEBUG
    puts("initEM: done");
#endif
    return 1;
}

int initValve(ValveList *list, const SensorList *sl, const EMList *el) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i, j;
    list->length = 0;
    list->item = NULL;
    snprintf(q, sizeof q, "select id, master_id, is_master, rain_sensor_id, em_id from " APP_NAME_STR ".valve order by id");
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return 0;
    }
    list->length = PQntuples(r);
    if (list->length > 0) {
        list->item = (Valve *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
            fputs("ERROR: initValve: failed to allocate memory\n", stderr);
            PQclear(r);
            return 0;
        }
        FORL{
            list->item[i].id = atoi(PQgetvalue(r, i, 0));
            list->item[i].master_id = atoi(PQgetvalue(r, i, 1));
            list->item[i].is_master = atoi(PQgetvalue(r, i, 2));
            list->item[i].sensor_rain = getSensorById(atoi(PQgetvalue(r, i, 3)), sl);
            list->item[i].em = getEMById(atoi(PQgetvalue(r, i, 4)), el);
            list->item[i].master = NULL;
            list->item[i].count = 0;
            if (!initMutex(&list->item[i].mutex_master)) {
                return 0;
            }
        }
        FORL{
            for (j = 0; j < list->length; j++) {
                if (list->item[i].master_id == list->item[j].id) {
                    list->item[i].master = &list->item[j];
                }
            }
        }
    }
    PQclear(r);
    if (!checkValve(list)) {
        return 0;
    }
#ifdef MODE_DEBUG
    puts("initValve: done");
#endif
    return 1;
}

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
        return 0;
    }
    if (list->top == NULL) {
        list->top = item;
    } else {
        list->last->next = item;
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("prog with id=%d loaded\n", item->valve->id);
#endif
    return 1;
}

int addProgById(int valve_id, ProgList *list) {
    Valve *rvalve = getRunningValveById(valve_id);
    if (rvalve != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "WARNING: addProgById: valve with id = %d is being controlled by program\n", rvalve->id);
#endif
        return 0;
    }
    Prog *p = getValveProgByIdFdb(valve_id, &valve_list);
    if (p == NULL) {
        return 0;
    }
    if (!addProg(p, list)) {
        free(p);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list) {
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->valve->id == id) {
            if (prev != NULL) {
                prev->next = curr->next;
            } else {//curr=top
                list->top = curr->next;
            }
            if (curr == list->last) {
                list->last = prev;
            }
            free(curr);
            list->length--;
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int switchProgById(int id, ProgList *list) {
    if (deleteProgById(id, list)) {
        return 1;
    }
    return addProgById(id, list);
}

void loadAllProg(ProgList *list, const ValveList *vlist) {
    PGresult *r;
    char *q = "select valve.id, extract(epoch from prog.busy_time) busy_time, extract(epoch from prog.idle_time) idle_time, prog.repeat, prog.busy_infinite, prog.repeat_infinite, prog.start_kind, prog.month_plan, prog.weekday_plan, prog.time_plan_id, prog.change_plan_id, valve.prev_id, valve.rain_sensitive, prog.id from " APP_NAME_STR ".valve, " APP_NAME_STR ".prog where valve.prog_id=prog.id and valve.is_master=0";
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return;
    }
    int n = PQntuples(r);
    int i;
    for (i = 0; i < n; i++) {
        int valve_id = atoi(PQgetvalue(r, 0, 0));
        Valve *valve = getValveById(valve_id, vlist);
        if (valve == NULL) {
            continue;
        }
        Prog *item = (Prog *) malloc(sizeof *(item));
        if (item == NULL) {
#ifdef MODE_DEBUG
            fputs("ERROR: loadAllProg: failed to allocate memory\n", stderr);
#endif
            break;
        }
        item->valve = valve;
        item->busy_time.tv_sec = (int) atof(PQgetvalue(r, 0, 1));
        item->busy_time.tv_nsec = 0;
        item->idle_time.tv_sec = (int) atof(PQgetvalue(r, 0, 2));
        item->idle_time.tv_nsec = 0;
        item->repeat = atoi(PQgetvalue(r, 0, 3));
        item->busy_infinite = atoi(PQgetvalue(r, 0, 4));
        item->repeat_infinite = atoi(PQgetvalue(r, 0, 5));
        memcpy(&item->start_kind, PQgetvalue(r, 0, 6), sizeof item->start_kind);
        memcpy(&item->month_plan, PQgetvalue(r, 0, 7), sizeof item->month_plan);
        memcpy(&item->weekday_plan, PQgetvalue(r, 0, 8), sizeof item->weekday_plan);
        item->tpl = getProgTimePlanListFdb(atoi(PQgetvalue(r, 0, 9)));
        item->cpl = getProgChangePlanListFdb(atoi(PQgetvalue(r, 0, 10)));
        item->prev_valve_id = atoi(PQgetvalue(r, 0, 11));
        item->rain_sensitive = atoi(PQgetvalue(r, 0, 12));
        item->id = atoi(PQgetvalue(r, 0, 13));
        item->state = INIT;
        item->next = NULL;
        if (!initMutex(&item->mutex)) {
            FREE_LIST(&item->tpl);
            FREE_LIST(&item->cpl);
            free(item);
            continue;
        }
        if (!initMutex(&item->mutex_all)) {
            FREE_LIST(&item->tpl);
            FREE_LIST(&item->cpl);
            free(item);
            continue;
        }
        if (!checkProg(item, list)) {
            FREE_LIST(&item->tpl);
            FREE_LIST(&item->cpl);
            free(item);
            continue;
        }
        if (!addProg(item, list)) {
            FREE_LIST(&item->tpl);
            FREE_LIST(&item->cpl);
            free(item);
            continue;
        }
    }
    PQclear(r);
    printProgList(&prog_list);
}

int readSensorRain(Sensor *s) {
    if (lockSensor(s)) {
        s->value = 0;
        unlockSensor(s);
        return 1;
    }
    return 0;
}

/*
int readSensorRain(Sensor *s) {
    if (s == NULL) {
        return 0;
    }
 struct timespec now=getCurrentTime();
 if(!timeHasPassed(rsens_interval_min, s->last_read_time, now)){
  return s->last_return;
  }
  
  
    int di[1];
    di[0] = s->remote_id;
    I1List data = {di, 1};

    if (!acp_sendBufArrPackI1List(ACP_CMD_GET_INT, udp_buf_size, &data, s->source)) {
#ifdef MODE_DEBUG
        fputs("ERROR: sensorRead: acp_sendBufArrPackI1List failed\n", stderr);
#endif
 s->last_return=0;
        return 0;
    }
    //waiting for response...
    I2 td[1];
    I2List tl = {td, 0};

    if (!acp_recvI2(&tl, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED, udp_buf_size, 1, *(s->source->fd))) {
#ifdef MODE_DEBUG
        fputs("ERROR: sensorRead: acp_recvI2() error\n", stderr);
#endif
  s->last_return=0;
        return 0;
    }
    if (tl.item[0].p0 != s->id) {
#ifdef MODE_DEBUG
        fputs("ERROR: sensorRead: response: id is not requested one\n", stderr);
#endif
 s->last_return=0;
        return 0;
    }
    s->value = tl.item[0].p1;
  s->last_read_time=now;
  s->last_return=1;
    return 1;
}
 */
int controlEM(EM *em, int output) {
    if (lockEM(em)) {
        em->last_output = (float) output;
        unlockEM(em);
        return 1;
    }
    return 0;
}

/*
int controlEM(EM *em, int output) {
    if (em == NULL) {
        return 0;
    }
    if (output == em->last_output) {
        return 0;
    }
    I2 di[1];
    di[0].p0 = em->remote_id;
    di[0].p1 = output;
    I2List data = {di, 1};
    if (!acp_sendBufArrPackI2List(ACP_CMD_SET_INT, udp_buf_size, &data, em->source)) {
#ifdef MODE_DEBUG
        fputs("ERROR: controlEM: failed to send request\n", stderr);
#endif
        return 0;
    }
    em->last_output =  (float) output;
    return 1;
}
 */

void serverRun(int *state, int init_state) {
    char buf_in[udp_buf_size];
    char buf_out[udp_buf_size];
    uint8_t crc;
    int i, j;
    char q[LINE_SIZE];
    crc = 0;
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(udp_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
    }
#ifdef MODE_DEBUG
    dumpBuf(buf_in, sizeof buf_in);
#endif    
    if (!crc_check(buf_in, sizeof buf_in)) {
#ifdef MODE_DEBUG
        fputs("ERROR: serverRun: crc check failed\n", stderr);
#endif
        return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_APP_START:
            if (!init_state) {
                *state = APP_INIT_DATA;
            }
            return;
        case ACP_CMD_APP_STOP:
            if (init_state) {
                waitThread_ctl(buf_in);
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            waitThread_ctl(buf_in);
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            waitThread_ctl(buf_in);
            *state = APP_EXIT;
            return;
        case ACP_CMD_APP_PING:
            if (init_state) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_BUSY);
            } else {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_IDLE);
            }
            return;
        case ACP_CMD_APP_PRINT:
            printAll(&prog_list, &peer_list, &em_list, &sensor_list, &valve_list);
            return;
        case ACP_CMD_APP_HELP:
            printHelp();
            return;
        case ACP_CMD_IRG_GET_TIME:
        {
            struct tm *current;
            time_t now;
            time(&now);
            current = localtime(&now);
            if (!bufCatDate(current, buf_out, udp_buf_size)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
        }
        default:
            if (!init_state) {
                return;
            }
            break;
    }

    switch (buf_in[0]) {
        case ACP_QUANTIFIER_BROADCAST:
        case ACP_QUANTIFIER_SPECIFIC:
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
        case ACP_CMD_START:
        case ACP_CMD_RESET:
        case ACP_CMD_IRG_PROG_MTURN:
        case ACP_CMD_IRG_VALVE_TURN_ON:
        case ACP_CMD_IRG_VALVE_TURN_OFF:
        case ACP_CMD_IRG_VALVE_GET_DATA:
        case ACP_CMD_IRG_VALVE_GET_DATA1:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &i1l, udp_buf_size);
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;

    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    if (lockProgList()) {
                        PROG_LIST_LOOP_ST
                        turnOFFV(curr->valve);
                        PROG_LIST_LOOP_SP
                        freeProg(&prog_list);
                        unlockProgList();
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    if (lockProgList()) {
                        for (i = 0; i < i1l.length; i++) {
                            Prog *p = getProgByValveId(i1l.item[i], &prog_list);
                            if (p != NULL) {
                                turnOFFV(p->valve);
                            }
                            deleteProgById(i1l.item[i], &prog_list);
                        }
                        unlockProgList();
                    }
                    break;
            }
            return;
        case ACP_CMD_START:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    if (lockProgList()) {
                        loadAllProg(&prog_list, &valve_list);
                        unlockProgList();
                    }
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    if (lockProgList()) {
                        for (i = 0; i < i1l.length; i++) {
                            addProgById(i1l.item[i], &prog_list);
                        }
                        unlockProgList();
                    }
                    break;
            }
            return;
        case ACP_CMD_RESET:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        curr->state = INIT;
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgByValveId(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->state = INIT;
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_IRG_PROG_MTURN:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        curr->toggle = 1;
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgByValveId(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->toggle = 1;
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_IRG_VALVE_TURN_ON:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    for (i = 0; i < valve_list.length; i++) {
                        turnONV(&valve_list.item[i]);
                        printf("turn on valve: %d\n", valve_list.item[i].id);
                    }
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Valve *p = getValveById(i1l.item[i], &valve_list);
                        if (p != NULL) {
                            turnONV(p);
                            printf("turn on valve: %d\n", p->id);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_IRG_VALVE_TURN_OFF:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    for (i = 0; i < valve_list.length; i++) {
                        turnOFFV(&valve_list.item[i]);
                        printf("turn off valve: %d\n", valve_list.item[i].id);
                    }
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Valve *p = getValveById(i1l.item[i], &valve_list);
                        if (p != NULL) {
                            turnOFFV(p);
                            printf("turn off valve: %d\n", p->id);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_IRG_VALVE_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < valve_list.length; i++) {
                        int done = 0;
                        Prog *prog = getProgByValveId(valve_list.item[i].id, &prog_list);
                        lockProg(prog);
                        lockEM(valve_list.item[i].em);
                        if (!bufCatValve(&valve_list.item[i], buf_out, udp_buf_size, prog)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            done = 1;
                        }
                        unlockProg(prog);
                        unlockEM(valve_list.item[i].em);
                        if (done) {
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        int done = 0;
                        Valve *valve = getValveById(i1l.item[i], &valve_list);
                        if (valve != NULL) {
                            Prog *prog = getProgByValveId(i1l.item[i], &prog_list);
                            lockProg(prog);
                            lockEM(valve->em);
                            if (!bufCatValve(valve, buf_out, udp_buf_size, prog)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                done = 1;
                            }
                            unlockProg(prog);
                            unlockEM(valve->em);
                        }
                        if (done) {
                            return;
                        }
                    }

                    break;
            }
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
        case ACP_CMD_IRG_VALVE_GET_DATA1:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < valve_list.length; i++) {
                        int done = 0;
                        Prog *prog = getProgByValveId(valve_list.item[i].id, &prog_list);
                        lockProg(prog);
                        lockEM(valve_list.item[i].em);
                        lockSensor(valve_list.item[i].sensor_rain);
                        if (!bufCatValve1(&valve_list.item[i], buf_out, udp_buf_size, prog)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            done = 1;
                        }
                        unlockProg(prog);
                        unlockEM(valve_list.item[i].em);
                        unlockSensor(valve_list.item[i].sensor_rain);
                        if (done) {
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        int done = 0;
                        Valve *valve = getValveById(i1l.item[i], &valve_list);
                        if (valve != NULL) {
                            Prog *prog = getProgByValveId(i1l.item[i], &prog_list);
                            lockProg(prog);
                            lockEM(valve->em);
                            lockSensor(valve->sensor_rain);
                            if (!bufCatValve1(valve, buf_out, udp_buf_size, prog)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                done = 1;
                            }
                            unlockProg(prog);
                            unlockEM(valve->em);
                            unlockSensor(valve->sensor_rain);
                        }
                        if (done) {
                            return;
                        }
                    }
                    break;
            }
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
    }

}

void secure() {
    size_t i;
    for (i = 0; i < valve_list.length; i++) {
        turnOFFV(&valve_list.item[i]);
    }
}

void turnOFFV(Valve *item) {
    if (item->em == NULL) {
        return;
    }
    if (item->master != NULL) {
        if (item->em->last_output == (float) TURN_ON) {
            if (lockValveMaster(item)) {
                if (item->master->count > 0) {
                    item->master->count--;
                }
                if (item->master->count == 0) {
                    controlEM(item->master->em, TURN_OFF);
                }
                unlockValveMaster(item);
            }
        }
    }
    controlEM(item->em, TURN_OFF);
}

void turnONV(Valve *item) {
    if (item->em == NULL) {
        return;
    }
    if (item->master != NULL) {
        if (item->em->last_output == (float) TURN_OFF) {
            if (lockValveMaster(item)) {
                item->master->count++;
                if (item->master->count == 1) {
                    controlEM(item->master->em, TURN_ON);
                }
                unlockValveMaster(item);
            }
        }
    }
    controlEM(item->em, TURN_ON);
}

void progControl(Prog *item) {
    switch (item->state) {
        case INIT:
            item->toggle = 0;
            item->tmr.ready = 0;
            item->state = CSKIND;
            item->state_wp = OFF;
            item->state_tc = INIT;
            item->state_rn = INIT;
            break;
        case CSKIND:
            item->state_wp = OFF;
            item->toggle = 0;
            item->crepeat = 0;
            switch (item->start_kind) {
                case KIND_TIME:
                    item->state = GTIME;
                    break;
                case KIND_PREV:
                    item->state_wp = CHECK;
                    item->state = WPREV;
                    break;
                case KIND_MANUAL:
                    item->state = WMANUAL;
                    break;
                default:
                    item->state = OFF;
                    break;
            }
            break;
        case GTIME:
            item->tp_i = getTimeIdFromTP(&item->tpl);
            if (item->tp_i == -1) {
                item->state = OFF;
            } else {
                item->state = WTIME;
            }
            break;
        case WTIME:
        {
            long int curr_tod = getCurrTOD();
            int result = todHasCome(item->tpl.item[item->tp_i], curr_tod);
            if (result == TARGET_OK) {
                item->state = CMONWD;
            }
            
/*
 * this method is slow but more clever, use it if your loop duration is more than GOOD_TOD_DELAY
 * using it you do not need GTIME
            if(timeFromTPHasCome(&item->tpl)){
                item->state = CMONWD;
            }
*/
            break;
        }
        case WPREV:
            ;
            Prog *p = getProgByValveId(item->prev_valve_id, &prog_list);
            if (p != NULL) {
                switch (item->state_wp) {
                    case OFF:
                        break;
                    case CHECK:
                        if (p->state == CREP || p->state == WBTIME || p->state == WITIME || p->state == WBINF) {
                            item->state_wp = WSTOP;
                        } else {
                            item->state_wp = WSTART;
                        }
                        break;
                    case WSTART:
                        if (p->state == CREP || p->state == WBTIME || p->state == WITIME || p->state == WBINF) {
                            item->state_wp = WSTOP;
                        }
                        break;
                    case WSTOP:
                        if (p->state != CREP && p->state != WBTIME && p->state != WITIME && p->state != WBINF) {
                            item->state_wp = OFF;
                            item->state = CMONWD;
                        }
                        break;
                }
            } else {
                item->state_wp = CHECK;
            }
            break;
        case WMANUAL:
            if (item->toggle) {
                item->toggle = 0;
                item->state = CMONWD;
            }
            break;
        case CMONWD:
            ;
            struct tm *current;
            time_t now;
            time(&now);
            current = localtime(&now);
            if (item->month_plan[current->tm_mon] == '1' && item->weekday_plan[current->tm_wday] == '1') {
                item->state = CREP;
            } else {
                item->state = CSKIND;
            }
            break;
        case CREP:
            if (item->repeat_infinite || (item->crepeat < item->repeat && item->crepeat >= 0)) {
                if (item->cbusy_time.tv_sec > 0 || item->cbusy_time.tv_nsec > 0) {
                    if (!item->blocked_rn) {
                        turnONV(item->valve);
                    }
                }
                if (item->busy_infinite) {
                    item->state = WBINF;
                } else {
                    item->state = WBTIME;
                }
            } else {
                item->state = CSKIND;
            }
            break;
        case WBTIME:
            if (ton_ts(item->cbusy_time, &item->tmr)) {
                turnOFFV(item->valve);
                item->state = WITIME;
            }
            break;
        case WITIME:
            if (ton_ts(item->idle_time, &item->tmr)) {
                if (!item->repeat_infinite) {
                    item->crepeat++;
                }
                item->state = CREP;
            }
            break;
        case WBINF:
            break;
        default:
            item->state = OFF;
            break;
    }
    switch (item->state_tc) {
        case OFF:
            break;
        case INIT:
            item->step_tc = 0;
            item->tmr_tc.ready = 0;
            item->cbusy_time = item->busy_time;
            if (item->busy_infinite) {
                item->state_tc = OFF;
            } else {
                item->state_tc = CSTEP;
            }
            break;
        case CSTEP:
            if (item->step_tc < item->cpl.length) {
                item->state_tc = WGAP;
            } else {
                item->state_tc = OFF;
            }
            break;
        case WGAP:
            if (ton_ts(item->cpl.item[item->step_tc].gap, &item->tmr_tc)) {
                item->state_tc = DO;
            }
            break;
        case DO:
        {
            struct timespec shift = {item->cpl.item[item->step_tc].shift, 0};
            timespecadd(&item->cbusy_time, &shift, &item->cbusy_time);
            item->step_tc++;
            item->state_tc = CSTEP;
            break;
        }
        default:
            item->state_tc = OFF;
            break;
    }
    switch (item->state_rn) {
        case OFF:
            break;
        case INIT:
            item->blocked_rn = 0;
            if (item->valve->sensor_rain == NULL) {
                item->state_rn = OFF;
                break;
            }
            if (item->rain_sensitive) {
                item->state_rn = WRAIN;
            } else {
                item->state_rn = OFF;
            }
            break;
        case WRAIN:
            readSensorRain(item->valve->sensor_rain);
            if (item->valve->sensor_rain->value) {
                item->state_rn = SRAIN;
            }
            break;
        case SRAIN:
            item->blocked_rn = 1;
            turnOFFV(item->valve);
            item->state_rn = WDRY;
            break;
        case WDRY:
            readSensorRain(item->valve->sensor_rain);
            if (!item->valve->sensor_rain->value) {
                item->state_rn = SDRY;
            }
            break;
        case SDRY:
            item->blocked_rn = 0;
            if (item->state == WBTIME || item->state == WBINF) {
                if (item->blocked_rn) {
                    turnONV(item->valve);
                }
            }
            item->state_rn = WRAIN;
            break;
        default:
            item->state_rn = OFF;
            break;
    }
}

void *threadFunction_ctl(void *arg) {
    ThreadData *data = (ThreadData *) arg;
    data->on = 1;
    int r;
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &r) != 0) {
        perror("threadFunction_ctl: pthread_setcancelstate");
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &r) != 0) {
        perror("threadFunction_ctl: pthread_setcanceltype");
    }
    secure();
    while (1) {
        size_t i;
        struct timespec t1 = getCurrentTime();
        // pingPeerList(peer_ping_interval, t1, &peer_list);
        if (tryLockProgList()) {
            PROG_LIST_LOOP_ST
            if (tryLockProgA(curr)) {
                if (tryLockProg(curr)) {
                    progControl(curr);
                    unlockProg(curr);
                }
                unlockProgA(curr);
            } else {
                break;
            }
            PROG_LIST_LOOP_SP
            unlockProgList();
        }
        switch (data->cmd) {
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                secure();
                data->cmd = ACP_CMD_APP_NO;
                data->on = 0;
                return (EXIT_SUCCESS);
        }
        data->cmd = ACP_CMD_APP_NO; //notify main thread that command has been executed
        sleepRest(data->cycle_duration, t1);
    }
}

int createThread_ctl(ThreadData * td) {
    //set attributes for each thread
    if (pthread_attr_init(&td->thread_attr) != 0) {
        perror("createThreads: pthread_attr_init");
        return 0;
    }
    td->attr_initialized = 1;
    td->cycle_duration = cycle_duration;
    if (pthread_attr_setdetachstate(&td->thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("createThreads: pthread_attr_setdetachstate");
        return 0;
    }

    //create a thread
    if (pthread_create(&td->thread, &td->thread_attr, threadFunction_ctl, (void *) td) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    td->created = 1;
    return 1;
}

void freeProg(ProgList *list) {
    Prog *curr = list->top, *temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        FREE_LIST(&temp->tpl);
        FREE_LIST(&temp->cpl);
        free(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void freeThread_ctl() {
    if (thread_data_ctl.created) {
        if (thread_data_ctl.on) {
            char cmd[2] = {ACP_QUANTIFIER_BROADCAST, ACP_CMD_APP_EXIT};
            waitThreadCmd(&thread_data_ctl.cmd, &thread_data_ctl.qfr, cmd);
        }
    }
    if (thread_data_ctl.attr_initialized) {
        if (pthread_attr_destroy(&thread_data_ctl.thread_attr) != 0) {
            perror("freeThread: pthread_attr_destroy");
        }
    }
    thread_data_ctl.on = 0;
    thread_data_ctl.cmd = ACP_CMD_APP_NO;
    thread_data_ctl.created = 0;
}

void freeData() {
    freeThread_ctl();
#ifdef MODE_DEBUG
    puts("freeData: freeThread done");
#endif    
    lockClose(&lock);
#ifdef MODE_DEBUG
    puts("freeData: lock closed");
#endif      
    freeProg(&prog_list);
    FREE_LIST(&i1l);
    FREE_LIST(&valve_list);
    FREE_LIST(&em_list);
    FREE_LIST(&sensor_list);
    FREE_LIST(&peer_list);
#ifdef MODE_DEBUG
    puts("freeData: lists a free");
#endif  
    freeDB(&db_conn_data);
#ifdef MODE_DEBUG
    puts("freeData: free db_conn_data done");
#endif  
    freeDB(&db_conn_public);
#ifdef MODE_DEBUG
    puts("freeData: free db_conn_public done");
#endif 
}

void freeApp() {
    freeData();
#ifdef MODE_DEBUG
    puts("freeData: done");
#endif
    freeSocketFd(&udp_fd);
#ifdef MODE_DEBUG
    puts("free udp_fd: done");
#endif
    freeSocketFd(&udp_fd_tf);
#ifdef MODE_DEBUG
    puts("udp_fd_tf: done");
#endif
    freeMutex(&progl_mutex);
#ifdef MODE_DEBUG
    puts("free progl_mutex: done");
#endif
    freePid(&pid_file, &proc_id, pid_path);
#ifdef MODE_DEBUG
    puts("freePid: done");
#endif
    freeDB(&db_conn_settings);
#ifdef MODE_DEBUG
    puts("free db_conn_settings: done");
#endif
#ifdef MODE_DEBUG
    puts("freeApp: done");
#endif
}

void exit_nicely() {
    freeApp();
    puts("\nBye...");
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                if (initData()) {
                    lockOpen(&lock);
                    data_initialized = 1;
                } else {
                    data_initialized = 0;
                }
                app_state = APP_RUN;
                break;
            case APP_RUN:puts("main: run");
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}