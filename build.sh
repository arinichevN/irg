
#!/bin/bash
APP=irg
APP_DBG=`printf "%s_dbg" "$APP"`
INST_DIR=/usr/sbin
CONF_DIR=/etc/controller
PID_DIR=/var/run

#lubuntu
#PSQL_I_DIR=-I/usr/include/postgresql

#xubuntu
PSQL_I_DIR=-I/opt/PostgreSQL/9.5/include 

PSQL_L_DIR=-L/opt/PostgreSQL/9.5/lib

MODE_DEBUG=-DMODE_DEBUG
PLATFORM_ALL=-DP_ALL
PLATFORM_A20=-DP_A20
NONE=""

function move_bin {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp bin $INST_DIR/$APP && \
	chmod a+x $INST_DIR/$APP && \
	chmod og-w $INST_DIR/$APP && \
	echo "Your $APP executable file: $INST_DIR/$APP";
}

function move_bin_dbg {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp $APP_DBG $INST_DIR/$APP_DBG && \
	chmod a+x $INST_DIR/$APP_DBG && \
	chmod og-w $INST_DIR/$APP_DBG && \
	echo "Your $APP executable file for debugging: $INST_DIR/$APP_DBG";
}

function move_conf {
	([ -d $CONF_DIR ] || mkdir $CONF_DIR) && \
	cp  main.conf $CONF_DIR/$APP.conf && \
	echo "Your $APP configuration file: $CONF_DIR/$APP.conf";
}

#your application will run on OS startup
function conf_autostart {
	cp -v starter_init /etc/init.d/$APP && \
	chmod 755 /etc/init.d/$APP && \
	update-rc.d -f $APP remove && \
	update-rc.d $APP defaults 30 && \
	echo "Autostart configured";
}

#    1         2        3     4
#platform debug_mode psql_I psql_L
function build {
	cd lib && \
	./build.sh $1 $2 $3 $4 && \
	cd ../ && \
	gcc -D_REENTRANT $1 $2 main.c -o $5 $3 $4 -L./lib -lpq -lpthread -lpac && \
	echo "Application successfully compiled. Launch command: sudo ./"$5
}

function for_all {
	build $PLATFORM_ALL $NONE $PSQL_I_DIR $PSQL_L_DIR $APP && \
	build $PLATFORM_ALL $MODE_DEBUG $PSQL_I_DIR $PSQL_L_DIR $APP_DBG && \
	move_bin && move_bin_dbg && move_conf && conf_autostart
}

function for_all_debug {
	build $PLATFORM_ALL $MODE_DEBUG $PSQL_I_DIR $PSQL_L_DIR $APP_DBG
}

function uninstall {
	pkill -F $PID_DIR/$APP.pid --signal 9
	update-rc.d -f $APP remove
	rm -f $INST_DIR/$APP
	rm -f $INST_DIR/$APP_DBG
	rm -f $CONF_DIR/$APP.conf
}

f=$1
${f} $2