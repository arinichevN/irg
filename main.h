
#ifndef IRG_H
#define IRG_H


#include "lib/db.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/gpio.h"
#include "lib/pid.h"
#include "lib/app.h"
#include "lib/config.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/irg.h"


#define APP_NAME irg
#define APP_NAME_STR TOSTRING(APP_NAME)


#ifndef MODE_DEBUG
#define CONF_FILE_P(APP) "/etc/controller/"#APP".conf"
#define CONF_FILE "/etc/controller/"APP_NAME_STR".conf"
#endif
#ifdef MODE_DEBUG
#define CONF_FILE "main.conf"
#endif

#define TURN_ON LOW
#define TURN_OFF HIGH

#define WAIT_RESP_TIMEOUT 1

#define MODE_STR_HEATER "h"
#define MODE_STR_COOLER "c"
#define STATE_STR_INIT "i"
#define STATE_STR_REG "r"
#define STATE_STR_TUNE "t"
#define UNKNOWN_STR "u"

#define KIND_TIME 't'
#define KIND_PREV 'p'
#define KIND_MANUAL 'm'
#define KIND_TIME_STR_STR "t"
#define KIND_PREV_STR "p"
#define KIND_MANUAL_STR "m"

#define PROG_LIST_LOOP_ST  {Prog *curr = prog_list.top; while (curr != NULL) {
#define PROG_LIST_LOOP_SP curr = curr->next; }} 

enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    CSKIND,
    CHECK,
    GTIME,
    WTIME,
    WPREV,
    WMANUAL,
    CMONWD,
    WSTART,
    WSTOP,
    CREP,
    WBTIME,
    WITIME,
    WBINF,
    CSTEP,
    WGAP,
    WRAIN,
    SRAIN,
    WDRY,
    SDRY
} StateAPP;

typedef struct {
    int id;
    int remote_id;
    Peer *source;
    float last_output; //we will keep last output value in order not to repeat the same queries to peers
    Mutex mutex;
} EM; //executive mechanism

DEF_LIST(EM)

typedef struct {
    int id;
    int remote_id;
    Peer *source;
    int value;
    Mutex mutex;
    struct timespec last_read_time;
    int last_return;
} Sensor;

DEF_LIST(Sensor)

typedef struct {
    char key[LOCK_KEY_SIZE];
    EM *em_list[LOCK_KEY_SIZE];
} Lock;

typedef struct {
    long int *item;
    size_t length;
} TimePlanList;

typedef struct {
    struct timespec gap;
    long int shift;
} ChangePlan;

DEF_LIST(ChangePlan)

typedef struct {
    char kind;
    int month;
    int day;
    long int tod;
} StSp;

struct valve_st {
    int id;
    struct valve_st *master;
    int master_id;
    int count; //used by master
    int is_master;
    EM *em;
    Sensor *sensor_rain;
    Mutex mutex_master;
};

typedef struct valve_st Valve;

DEF_LIST(Valve)

struct prog_st {
    int id;
    Valve *valve;
    struct timespec busy_time;
    struct timespec idle_time;
    int repeat;
    int busy_infinite;
    int repeat_infinite;
    char month_plan[12];
    char weekday_plan[7];
    char start_kind;
    int prev_valve_id;
    int rain_sensitive;
    ChangePlanList cpl;
    TimePlanList tpl;

    char state;
    char state_wp;
    char state_rn;
    char state_tc; //time change
    int step_tc;
    Ton_ts tmr_tc;
    Ton_ts tmr;
    size_t tp_i;
    int crepeat;
    int toggle;
    int blocked_rn;
    struct timespec cbusy_time;

    Mutex mutex;
    Mutex mutex_all;

    struct prog_st *next;
};

typedef struct prog_st Prog;

DEF_LLIST(Prog)

typedef struct {
    pthread_attr_t thread_attr;
    pthread_t thread;
    char cmd;
    char qfr;
    int on;
    struct timespec cycle_duration; //one cycle minimum duration
    int created;
    int attr_initialized;
} ThreadData;

extern int readSettings();

extern int initLock(Lock *item, const EMList *el);

extern int checkLock(Lock *item);

extern void lockOpen(Lock *item);

extern void lockClose(Lock *item);

extern void initApp();

extern int initData();

extern int initSensor(SensorList *list, const PeerList *pl);

extern int initEM(EMList *list, const PeerList *pl);

extern int initValve(ValveList *list, const SensorList *sl, const EMList *el);

extern int addProg(Prog *item, ProgList *list);

extern int addProgById(int valve_id, ProgList *list) ;

extern int deleteProgById(int id, ProgList *list);

extern int switchProgById(int id, ProgList *list);

extern void loadAllProg(ProgList *list, const ValveList *vlist);

extern int readSensorRain(Sensor *s);

extern int controlEM(EM *em, int output);

extern void serverRun(int *state, int init_state);

extern void secure();

extern void turnOFFV(Valve *item);

extern void turnONV(Valve *item);

extern void progControl(Prog *item);

extern void *threadFunction_ctl(void *arg);

extern int createThread_ctl(ThreadData * td);

extern void freeProg(ProgList *list);

extern void freeThread_ctl();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);
#endif 

