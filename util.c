
#include "main.h"

FUN_LIST_GET_BY_IDSTR(Peer)
FUN_LIST_GET_BY_ID(Sensor)
FUN_LIST_GET_BY_ID(EM)
FUN_LIST_GET_BY_ID(Valve)

Valve * getRunningValveById(int id) {
    PROG_LIST_LOOP_ST
    if (id == curr->valve->id) {
        return curr->valve;
    }
    PROG_LIST_LOOP_SP
    return NULL;
}

Prog *getProgByValveId(int id, const ProgList *list) {
    Prog *curr = list->top;
    while (curr != NULL) {
        if (curr->valve->id == id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

char * getStateStr(char in) {
    static char *str;
    switch (in) {
        case ON: str = "ON";
            break;
        case OFF: str = "OFF";
            break;
        case DO: str = "DO";
            break;
        case INIT: str = "INIT";
            break;
        case CSKIND: str = "CSKIND";
            break;
        case CHECK: str = "CHECK";
            break;
        case GTIME: str = "GTIME";
            break;
        case WTIME: str = "WTIME";
            break;
        case WPREV: str = "WPREV";
            break;
        case WMANUAL: str = "WMANUAL";
            break;
        case CMONWD: str = "CMONWD";
            break;
        case WSTART: str = "WSTART";
            break;
        case WSTOP: str = "WSTOP";
            break;
        case CREP: str = "CREP";
            break;
        case WBTIME: str = "WBTIME";
            break;
        case WITIME: str = "WITIME";
            break;
        case WBINF: str = "WBINF";
            break;
        case CSTEP: str = "CSTEP";
            break;
        case WGAP: str = "WGAP";
            break;
        case WRAIN: str = "WRAIN";
            break;
        case SRAIN: str = "SRAIN";
            break;
        case WDRY: str = "WDRY";
            break;
        case SDRY: str = "SDRY";
            break;
        default: str = "?";
            break;
    }
    return str;
}

int timeFromTPHasCome(TimePlanList *list) {
    long int curr_tod = getCurrTOD();
    size_t i;
    FORL{
        int result = todHasCome(list->item[i], curr_tod);
        if (result == TARGET_AHEAD) {
            continue;
        } else if (result == TARGET_OK) {
            return 1;
        } else if (result == TARGET_BEHIND) {
            break;
        }
    }
    return 0;
}

size_t getTimeIdFromTP(TimePlanList *list) {
    long int curr_tod = getCurrTOD();
    size_t i;
    int found = 0;
    FORL{
        /*
                int result = todHasCome(list->item[i], curr_tod);
                if (result == TARGET_BEHIND) {
                    found = 1;
                    break;
                }
         */
        if (curr_tod <= LIi) {
            found = 1;
            break;
        }
    }
    if (found) {
        return i;
    } else {
        if (list->length > 0) {
            return 0;
        } else {
            return -1;
        }
    }
    return 0;
}

int checkSensor(const SensorList *list) {
    size_t i, j;
    FORL{
        if (list->item[i].source == NULL) {
            fprintf(stderr, "ERROR: checkSensorList: no data source where id = %d\n", list->item[i].id);
            return 0;
        }
    }
    //unique id
    FORL{
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id == list->item[j].id) {
                fprintf(stderr, "ERROR: checkSensorList: id is not unique where id = %d\n", list->item[i].id);
                return 0;
            }
        }
    }
    return 1;
}

int checkEM(const EMList *list) {
    size_t i, j;
    FORL{
        if (list->item[i].source == NULL) {
            fprintf(stderr, "ERROR: checkEm: no data source where id = %d\n", list->item[i].id);
            return 0;
        }
    }
    //unique id
    FORL{
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id == list->item[j].id) {
                fprintf(stderr, "ERROR: checkEm: id is not unique where id = %d\n", list->item[i].id);
                return 0;
            }
        }
    }
    return 1;
}

int checkValve(const ValveList *list) {
    size_t i, j;
    FORL{
        if (list->item[i].is_master != 1 && list->item[i].is_master != 0) {
            fprintf(stderr, "ERROR: checkValve: bad is_master (1 or 0 expected) where id = %d\n", list->item[i].id);
            return 0;
        }
    }
    //unique id
    FORL{
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id == list->item[j].id) {
                fprintf(stderr, "ERROR: checkValve: id is not unique where id = %d\n", list->item[i].id);
                return 0;
            }
        }
    }
    return 1;
}

int checkTimePlan(const TimePlanList *list) {
    size_t i, j;
    FORL{
        if (list->item[i] < 0 || list->item[i] >= 86400) {
            fprintf(stderr, "ERROR: checkTimePlan: bad start_time where start_time=%ld\n", list->item[i]);
            return 0;
        }
    }
    //unique (id  start time)
    FORL{
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i] == list->item[j]) {
                fprintf(stderr, "ERROR: checkTimePlan: start_time is not unique where start_time=%ld\n", list->item[i]);
                return 0;
            }
        }
    }
    return 1;
}

int checkChangePlan(const ChangePlanList *list) {
    size_t i, j;
    FORL{
        if (list->item[i].gap.tv_sec < 0 || list->item[i].gap.tv_nsec < 0) {
            fprintf(stderr, "ERROR: checkTimePlan: bad gap where gap = %ld and shift=%ld\n", list->item[i].gap.tv_sec, list->item[i].shift);
            return 0;
        }
    }
    return 1;
}

int checkProg(const Prog *item, const ProgList *list) {
    if (item->busy_time.tv_sec < 0 || item->busy_time.tv_nsec < 0) {
        fprintf(stderr, "ERROR: checkProg: negative busy_time where valve id = %d\n", item->valve->id);
        return 0;
    }
    if (item->idle_time.tv_sec < 0 || item->idle_time.tv_nsec < 0) {
        fprintf(stderr, "ERROR: checkProg: negative idle_time where valve id = %d\n", item->valve->id);
        return 0;
    }
    switch (item->start_kind) {
        case KIND_TIME:
        case KIND_PREV:
        case KIND_MANUAL:
            break;
        default:
            fprintf(stderr, "ERROR: checkProg: bad start_kind where valve id = %d\n", item->valve->id);
            return 0;
    }
    int i;
    for (i = 0; i < MONTH_NUM; i++) {
        if (item->month_plan[i] != '1' && item->month_plan[i] != '0') {
            fprintf(stderr, "ERROR: checkProg: bad month_plan where valve id = %d\n", item->valve->id);
            return 0;
        }
    }
    for (i = 0; i < WDAY_NUM; i++) {
        if (item->weekday_plan[i] != '1' && item->weekday_plan[i] != '0') {
            fprintf(stderr, "ERROR: checkProg: bad weekday_plan where valve id = %d\n", item->valve->id);
            return 0;
        }
    }
    if (item->repeat < 0) {
        fprintf(stderr, "ERROR: checkProg: negative repeat where valve id = %d\n", item->valve->id);
        return 0;
    }
    //unique id
    if (getProgByValveId(item->valve->id, list) != NULL) {
        fprintf(stderr, "ERROR: checkProg: prog for valve with id = %d is already running\n", item->valve->id);
        return 0;
    }
    return 1;
}

TimePlanList getProgTimePlanListFdb(int id) {
    TimePlanList list = {NULL, 0};
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select start_time from " APP_NAME_STR ".time_plan where id=%d order by start_time asc", id);
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return list;
    }
    list.length = PQntuples(r);
    if (list.length > 0) {
        list.item = (long int *) malloc(list.length * sizeof *(list.item));
        if (list.item == NULL) {
            list.length = 0;
#ifdef MODE_DEBUG
            fputs("ERROR: getProgTimePlanListFdb: failed to allocate memory\n", stderr);
#endif
            PQclear(r);
            list.length = 0;
            return list;
        }
        size_t i;
        for (i = 0; i < list.length; i++) {
            list.item[i] = atoi(PQgetvalue(r, i, 0));
        }
    }
    PQclear(r);
    if (!checkTimePlan(&list)) {
        free(list.item);
        list.item = NULL;
        list.length = 0;
        return list;
    }
    return list;
}

ChangePlanList getProgChangePlanListFdb(int id) {
    ChangePlanList list = {NULL, 0};
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select extract(epoch from gap) gap, shift from " APP_NAME_STR ".change_plan where id=%d order by seq asc", id);
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return list;
    }
    list.length = PQntuples(r);
    if (list.length > 0) {
        list.item = (ChangePlan *) malloc(list.length * sizeof *(list.item));
        if (list.item == NULL) {
            list.length = 0;
#ifdef MODE_DEBUG
            fputs("ERROR: getProgChangePlanListFdb: failed to allocate memory\n", stderr);
#endif
            PQclear(r);
            list.length = 0;
            return list;
        }
        size_t i;
        for (i = 0; i < list.length; i++) {
            list.item[i].gap.tv_sec = (int) atof(PQgetvalue(r, i, 0));
            list.item[i].gap.tv_nsec = 0;
            list.item[i].shift = atoi(PQgetvalue(r, i, 1));
        }
    }
    PQclear(r);
    if (!checkChangePlan(&list)) {
        free(list.item);
        list.item = NULL;
        list.length = 0;
        return list;
    }
    return list;
}

Prog * getValveProgByIdFdb(int valve_id, const ValveList *vlist) {
    Valve *valve = getValveById(valve_id, vlist);
    if (valve == NULL) {
        return NULL;
    }
    PGresult *r;
    char q[LINE_SIZE * 2];
    snprintf(q, sizeof q, "select extract(epoch from prog.busy_time) busy_time, extract(epoch from prog.idle_time) idle_time, prog.repeat, prog.busy_infinite, prog.repeat_infinite, prog.start_kind, prog.month_plan, prog.weekday_plan, prog.time_plan_id, prog.change_plan_id, valve.prev_id, valve.rain_sensitive, prog.id from " APP_NAME_STR ".valve, " APP_NAME_STR ".prog where valve.prog_id=prog.id and valve.is_master=0 and valve.id=%d", valve_id);
    if ((r = dbGetDataT(*db_connp_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("ERROR: getProgByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return NULL;
    }

    Prog *item = (Prog *) malloc(sizeof *(item));
    if (item == NULL) {
#ifdef MODE_DEBUG
        fputs("ERROR: getProgByIdFdb: failed to allocate memory\n", stderr);
#endif
        PQclear(r);
        return NULL;
    }
    item->valve = valve;
    item->busy_time.tv_sec = (int) atof(PQgetvalue(r, 0, 0));
    item->busy_time.tv_nsec = 0;
    item->idle_time.tv_sec = (int) atof(PQgetvalue(r, 0, 1));
    item->idle_time.tv_nsec = 0;
    item->repeat = atoi(PQgetvalue(r, 0, 2));
    item->busy_infinite = atoi(PQgetvalue(r, 0, 3));
    item->repeat_infinite = atoi(PQgetvalue(r, 0, 4));
    memcpy(&item->start_kind, PQgetvalue(r, 0, 5), sizeof item->start_kind);
    memcpy(&item->month_plan, PQgetvalue(r, 0, 6), sizeof item->month_plan);
    memcpy(&item->weekday_plan, PQgetvalue(r, 0, 7), sizeof item->weekday_plan);
    item->tpl = getProgTimePlanListFdb(atoi(PQgetvalue(r, 0, 8)));
    item->cpl = getProgChangePlanListFdb(atoi(PQgetvalue(r, 0, 9)));
    item->prev_valve_id = atoi(PQgetvalue(r, 0, 10));
    item->rain_sensitive = atoi(PQgetvalue(r, 0, 11));
    item->id = atoi(PQgetvalue(r, 0, 12));
    item->state = INIT;
    item->next = NULL;
    PQclear(r);

    if (!initMutex(&item->mutex)) {
        FREE_LIST(&item->tpl);
        FREE_LIST(&item->cpl);
        free(item);
        return NULL;
    }
    if (!checkProg(item, &prog_list)) {
        FREE_LIST(&item->tpl);
        FREE_LIST(&item->cpl);
        free(item);
        return NULL;
    }
    return item;
}

struct timespec getTimePassedMain(const Prog *item) {
    struct timespec dif;
    if (item->state == WBTIME || item->state == WITIME) {
        dif = getTimePassed_ts(item->tmr.start);
    } else {
        dif.tv_sec = -1;
        dif.tv_nsec = -1;
    }
    return dif;
}

struct timespec getTimeSpecifiedMain(const Prog *item) {
    struct timespec dif;
    switch (item->state) {
        case WBTIME:
            dif = item->cbusy_time;
            break;
        case WITIME:
            dif = item->idle_time;
            break;
        case WTIME:
            dif.tv_sec = item->tpl.item[item->tp_i];
            dif.tv_nsec = 0;
            break;
        default:
            dif.tv_sec = -1;
            dif.tv_nsec = -1;
            break;
    }
    return dif;
}

struct timespec getTimePassedTc(const Prog *item) {
    struct timespec out;
    if (item->state_tc == WGAP) {
        out = getTimePassed_ts(item->tmr_tc.start);
    } else {
        out.tv_sec = -1;
        out.tv_nsec = -1;
    }
    return out;
}

struct timespec getTimeRestTc(const Prog *item) {
    struct timespec out;
    if (item->state_tc == WGAP && item->step_tc < item->cpl.length) {
        out = getTimeRest_ts(item->cpl.item[item->step_tc].gap, item->tmr_tc.start);
    } else {
        out.tv_sec = -1;
        out.tv_nsec = -1;
    }
    return out;
}

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG

        perror("ERROR: lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {

        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG

        perror("ERROR: unlockProgList: error unlocking mutex");
#endif 
        return 0;
    }
    return 1;
}

FUN_LOCK(Prog)

FUN_LOCK(EM)

FUN_LOCK(Sensor)

FUN_LOCK(Peer)

FUN_TRYLOCK(Peer)

int lockValveMaster(Valve *item) {
    if (item == NULL || item->master == NULL) {
        return 0;
    }
    if (pthread_mutex_lock(&(item->master->mutex_master.self)) != 0) {
#ifdef MODE_DEBUG
        perror("ERROR: lockValveMaster: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProg(Prog *item) {
    if (item == NULL) {
        return 0;
    }
    if (pthread_mutex_trylock(&(item->mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

FUN_UNLOCK(Prog)

FUN_UNLOCK(EM)

FUN_UNLOCK(Sensor)

FUN_UNLOCK(Peer)

int unlockValveMaster(Valve *item) {
    if (item == NULL || item->master == NULL) {
        return 0;
    }
    if (pthread_mutex_unlock(&(item->master->mutex_master.self)) != 0) {
#ifdef MODE_DEBUG
        perror("ERROR: unlockValveMaster: error unlocking mutex");
#endif 
        return 0;
    }
    return 1;
}

void pingPeerList(struct timespec interval, struct timespec now, PeerList *list) {
    size_t i;
    FORL{
        if (lockPeer(&LIi)) {
            if (timeHasPassed(interval, LIi.time1, now)) {
                char cmd_str[1] = {ACP_CMD_APP_PING};
                if (!acp_sendStrPack(ACP_QUANTIFIER_BROADCAST, cmd_str, udp_buf_size, &LIi)) {
#ifdef MODE_DEBUG
                    fputs("ERROR: pingPeerList: acp_sendStrPack failed\n", stderr);
#endif
                    LIi.active = 0;
                    LIi.time1 = now;
                    unlockPeer(&LIi);
                    break;
                }
                //waiting for response...
                char resp[2] = {'\0', '\0'};
                resp[0] = acp_recvPing(&LIi, udp_buf_size);
                if (strncmp(resp, ACP_RESP_APP_BUSY, 1) != 0) {
#ifdef MODE_DEBUG
                    fputs("ERROR: pingPeerList: acp_recvPing() peer is not busy\n", stderr);
#endif
                    LIi.active = 0;
                    LIi.time1 = now;
                    unlockPeer(&LIi);
                    break;
                }
                LIi.active = 1;
                LIi.time1 = now;
            }
            unlockPeer(&LIi);
        }
    }
}

char *getValveProgRainState(const Valve *valve, const Prog *prog) {
    if (valve == NULL || prog == NULL) {
        return "NULL";
    }
    if (prog->rain_sensitive) {
        if (valve->sensor_rain != NULL) {
            if (valve->sensor_rain->value==0) {
                return "YES";
            } else {
                return "NO";
            }
        }
    }
    return "OFF";
}

int bufCatValve(const Valve *item, char *buf, size_t buf_size, const Prog *prog) {
    char q[LINE_SIZE];
    float output = -1.0f;
    if (item->em != NULL) {
        output = item->em->last_output;
    }
    if (prog == NULL) {
        snprintf(q, sizeof q, "%d_%s_%s_%s_%s_%d_%d_%d_%ld_%ld_%ld_%f\n",
                item->id,
                "NULL",
                "NULL",
                "NULL",
                "NULL",
                0,
                0,
                0,
                0L,
                0L,
                0L,
                output
                );
    } else {
        struct timespec dif_tc, dif;
        dif = getTimePassedMain(prog);
        dif_tc = getTimePassedTc(prog);
        snprintf(q, sizeof q, "%d_%s_%s_%s_%s_%d_%d_%d_%ld_%ld_%ld_%f\n",
                item->id,
                getStateStr(prog->state),
                getStateStr(prog->state_wp),
                getStateStr(prog->state_rn),
                getStateStr(prog->state_tc),
                prog->step_tc,
                prog->crepeat,
                prog->blocked_rn,
                prog->cbusy_time.tv_sec,
                dif.tv_sec,
                dif_tc.tv_sec,
                output

                );
    }

    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatValve1(const Valve *item, char *buf, size_t buf_size, const Prog *prog) {
    char q[LINE_SIZE];
    char *rain = getValveProgRainState(item, prog);
    float output = -1.0f;
    int em_peer_active = 0;
    if (item->em != NULL) {
        output = item->em->last_output;
        if (item->em->source != NULL) {
            em_peer_active = item->em->source->active;
        }
    }
    if (prog == NULL) {
        snprintf(q, sizeof q, "%d_%f_%s_%d_%d_%d_%d_%s_%s_%s_%s_%d_%d_%ld_%ld_%ld_%d\n",
                item->id,
                output,
                rain,
                item->is_master,
                item->count,

                0,
                0,
                "NULL",
                "NULL",
                "NULL",
                "NULL",
                -1,
                -1,
                -1L,
                -1L,
                -1L,
                em_peer_active
                );
    } else {
        struct timespec t_spec, t_pas, rest_tc;
        t_pas = getTimePassedMain(prog);
        t_spec = getTimeSpecifiedMain(prog);
        rest_tc = getTimeRestTc(prog);
        snprintf(q, sizeof q, "%d_%f_%s_%d_%d_%d_%d_%s_%s_%s_%s_%d_%d_%ld_%ld_%ld_%d\n",
                item->id,
                output,
                rain,
                item->is_master,
                item->count,

                prog->id,
                1,
                getStateStr(prog->state),
                getStateStr(prog->state_wp),
                getStateStr(prog->state_rn),
                getStateStr(prog->state_tc),
                prog->crepeat,
                prog->blocked_rn,
                t_pas.tv_sec,
                t_spec.tv_sec,
                rest_tc.tv_sec,
                em_peer_active
                );
    }

    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatDate(struct tm *date, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d_%d_%d_%d_%d_%d\n",
            date->tm_year,
            date->tm_mon,
            date->tm_mday,
            date->tm_hour,
            date->tm_min,
            date->tm_sec
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int sendStrPack(char qnf, char *cmd) {
    extern size_t udp_buf_size;
    extern Peer peer_client;

    return acp_sendStrPack(qnf, cmd, udp_buf_size, &peer_client);
}

int sendBufPack(char *buf, char qnf, const char *cmd_str) {
    extern size_t udp_buf_size;
    extern Peer peer_client;

    return acp_sendBufPack(buf, qnf, cmd_str, udp_buf_size, &peer_client);
}

void waitThread_ctl(char *cmd) {
    if (thread_data_ctl.on) {
        waitThreadCmd(&thread_data_ctl.cmd, &thread_data_ctl.qfr, cmd);
    }
}

void sendStr(const char *s, uint8_t *crc) {

    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {

    acp_sendFooter(crc, &peer_client);
}

char * getKind(char in) {
    static char *str;
    switch (in) {
        case KIND_TIME:
            str = "by time";
            break;
        case KIND_PREV:
            str = "after prev";
            break;
        case KIND_MANUAL:
            str = "manual";
            break;
        default:
            str = "?";
            break;
    }
    return str;
}

void printAll(ProgList *list, PeerList *pl, EMList *el, SensorList *sl, ValveList *vl) {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    size_t i;

    sendStr("+-------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                Peer                                             |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);
    sendStr("|               id               |  udp_port |      addr      |     fd    |  active   |   link    |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < pl->length; i++) {
        snprintf(q, sizeof q, "|%32s|%11u|%16u|%11d|%11d|%11p|\n",
                pl->item[i].id,
                pl->item[i].addr.sin_port,
                pl->item[i].addr.sin_addr.s_addr,
                *pl->item[i].fd,
                pl->item[i].active,
                &pl->item[i]
                );
        sendStr(q, &crc);
    }
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------+\n", &crc);
    sendStr("|                           EM                              |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    | remote_id | peer_link |  output   |   link    |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < el->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%11p|%11.1f|%11p|\n",
                el->item[i].id,
                el->item[i].remote_id,
                el->item[i].source,
                el->item[i].last_output,
                &el->item[i]
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------+\n", &crc);
    sendStr("|                           Sensor                          |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    | remote_id | peer_link |  value    |   link    |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < sl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%11p|%11d|%11p|\n",
                sl->item[i].id,
                sl->item[i].remote_id,
                sl->item[i].source,
                sl->item[i].value,
                &sl->item[i]
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    snprintf(q, sizeof q, "Lock key: %.32s\n", lock.key);
    sendStr(q, &crc);
    sendStr("+-----------+\n", &crc);
    sendStr("|  Lock_em  |\n", &crc);
    sendStr("+-----------+\n", &crc);
    sendStr("|  em_link  |\n", &crc);
    sendStr("+-----------+\n", &crc);
    for (i = 0; i < LOCK_KEY_SIZE; i++) {
        if (lock.em_list[i] != NULL) {
            snprintf(q, sizeof q, "|%11p|\n",
                    lock.em_list[i]
                    );
            sendStr(q, &crc);
        }
    }
    sendStr("+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                     Valve                                         |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    | master_id |   master  | is_master |   count   |  em_link  |sensor_link|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < vl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%11p|%11d|%11d|%11p|%11p|\n",
                vl->item[i].id,
                vl->item[i].master_id,
                vl->item[i].master,
                vl->item[i].is_master,
                vl->item[i].count,
                vl->item[i].em,
                vl->item[i].sensor_rain
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-------------------------------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                       Program                                                                 |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+----------+---------+------------+----------+----------+-----------+-----------+\n", &crc);
    sendStr("| valve_id  | busy_time | idle_time |  repeat   | busy_inf | rep_inf |  month_pl  |weekday_pl|start_kind| prev_id   | rain_sens |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+----------+---------+------------+----------+----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11ld|%11ld|%11d|%10d|%9d|%12.12s|%10.7s|%10s|%11d|%11d|\n",
            curr->valve->id,
            curr->busy_time.tv_sec,
            curr->idle_time.tv_sec,
            curr->repeat,
            curr->busy_infinite,
            curr->repeat_infinite,
            curr->month_plan,
            curr->weekday_plan,
            getStateStr(curr->start_kind),
            curr->prev_valve_id,
            curr->rain_sensitive
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+----------+---------+------------+----------+----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                                Program runtime                                                    |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|  valve_id |   state   | state_wp  | state_rn  | state_tc  | step_tc   | crepeat   | blocked_rn| cbusy_time| bi_time_ps|gap_time_ps|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
            struct timespec dif_tc, dif;
    dif = getTimePassedMain(curr);
    dif_tc = getTimePassedTc(curr);
    snprintf(q, sizeof q, "|%11d|%11s|%11s|%11s|%11s|%11d|%11d|%11d|%11ld|%11ld|%11ld|\n",
            curr->valve->id,
            getStateStr(curr->state),
            getStateStr(curr->state_wp),
            getStateStr(curr->state_rn),
            getStateStr(curr->state_tc),
            curr->step_tc,
            curr->crepeat,
            curr->blocked_rn,
            curr->cbusy_time.tv_sec,
            dif.tv_sec,
            dif_tc.tv_sec
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------+\n", &crc);
    sendStr("|  Program time plan    |\n", &crc);
    sendStr("+-----------+-----------+\n", &crc);
    sendStr("|  valve_id | start_time|\n", &crc);
    sendStr("+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    for (i = 0; i < curr->tpl.length; i++) {
        snprintf(q, sizeof q, "|%11d|%11ld|\n",
                curr->valve->id,
                curr->tpl.item[i]
                );
        sendStr(q, &crc);
    }
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------+\n", &crc);
    sendStr("|        Program change plan        |\n", &crc);
    sendStr("+-----------+-----------+-----------+\n", &crc);
    sendStr("|  valve_id |    gap    |   shift   |\n", &crc);
    sendStr("+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    for (i = 0; i < curr->cpl.length; i++) {
        snprintf(q, sizeof q, "|%11d|%11ld|%11ld|\n",
                curr->valve->id,
                curr->cpl.item[i].gap.tv_sec,
                curr->cpl.item[i].shift
                );
        sendStr(q, &crc);
    }
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+\n", &crc);

    sendFooter(crc);
}

void printProgList(ProgList *list) {
    puts("Prog list");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+----------+---------+------------+-----------+-----------+-----------+-----------+");
    puts("| valve_id  | snr_id    |  em_id    | master_id | busy_time | idle_time |  repeat   | busy_inf | rep_inf |  month_pl  | weekday_pl|start_kind | prev_id   | rain_sens |");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+----------+---------+------------+-----------+-----------+-----------+-----------+");
    PROG_LIST_LOOP_ST
    printf("|%11d|%11ld|%11ld|%11d|%10d|%9d|%12.12s|%11.7s|%11s|%11d|%11d|\n",
            curr->valve->id,
            curr->busy_time.tv_sec,
            curr->idle_time.tv_sec,
            curr->repeat,
            curr->busy_infinite,
            curr->repeat_infinite,
            curr->month_plan,
            curr->weekday_plan,
            getStateStr(curr->start_kind),
            curr->prev_valve_id,
            curr->rain_sensitive
            );
    PROG_LIST_LOOP_SP
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+----------+---------+------------+-----------+-----------+-----------+-----------+");
}

void printProgTimePlanList(ProgList *list) {
    size_t i;
    puts("time plan list");
    puts("+-----------+-----------+");
    puts("| valve_id  | start_time|");
    puts("+-----------+-----------+");
    PROG_LIST_LOOP_ST
    for (i = 0; i < curr->tpl.length; i++) {
        printf("|%11d|%11ld|\n",
                curr->valve->id,
                curr->tpl.item[i]
                );
    }
    PROG_LIST_LOOP_SP
    puts("+-----------+-----------+");
}

void printProgChangePlanList(ProgList *list) {
    size_t i;
    puts("change plan list");
    puts("+-----------+-----------+-----------+");
    puts("| valve_id  |   gap     |  shift    |");
    puts("+-----------+-----------+-----------+");
    PROG_LIST_LOOP_ST
    for (i = 0; i < curr->cpl.length; i++) {
        printf("|%11d|%11ld|%11ld|\n",
                curr->valve->id,
                curr->cpl.item[i].gap.tv_sec,
                curr->cpl.item[i].shift
                );
    }
    PROG_LIST_LOOP_SP
    puts("+-----------+-----------+-----------+");
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst put process in standby and then in active mode\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget system time were process is running\n", ACP_CMD_IRG_GET_TIME);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tload valve program into RAM and start its execution; valve id expected if '.' quantifier is used\n", ACP_CMD_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload valve program from RAM; valve id expected if '.' quantifier is used\n", ACP_CMD_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\texecute valve program from the beginning; no data reloading; valve id expected if '.' quantifier is used\n", ACP_CMD_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tturn on specified valve; valve id expected if '.' quantifier is used\n", ACP_CMD_IRG_VALVE_TURN_ON);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tturn off specified valve; valve id expected if '.' quantifier is used\n", ACP_CMD_IRG_VALVE_TURN_OFF);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tturn on main cycle; valve id expected if '.' quantifier is used\n", ACP_CMD_IRG_PROG_MTURN);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget program info; response format: valveId_progState_wprevState_rainState_tcState_tcStep_repeat_blockedRain_cBusyTime_busyIdleTimePassed_tcGapTimePassed; valve id expected if '.' quantifier is used\n", ACP_CMD_IRG_VALVE_GET_DATA);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget program info; response format: valveId_valveEMState_rainState_timePassed_timeSpecified_progId_isMaster_progLoaded; valve id expected if '.' quantifier is used\n", ACP_CMD_IRG_VALVE_GET_DATA1);
    sendStr(q, &crc);
    sendFooter(crc);
}
