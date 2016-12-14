
DROP SCHEMA if exists irg CASCADE;
CREATE SCHEMA irg;


CREATE TABLE irg.config
(
  app_class character varying(32) NOT NULL,
  db_public character varying(256) NOT NULL,
  udp_port character varying(32) NOT NULL,
  pid_path character varying(32) NOT NULL,
  udp_buf_size character varying(32) NOT NULL,
  db_data character varying(32) NOT NULL,
  cycle_duration_us character varying(32) NOT NULL,
  lock_key character varying(32) NOT NULL,
  CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);


CREATE TABLE irg.prog
(
  id integer NOT NULL,
  name character varying(32) NOT NULL DEFAULT 'prog',--not used by regulator, use it as description
  busy_time interval NOT NULL DEFAULT '60',
  idle_time interval NOT NULL DEFAULT '0',
  repeat integer NOT NULL DEFAULT 1,
  busy_infinite integer NOT NULL DEFAULT 0,
  repeat_infinite integer NOT NULL DEFAULT 0,
  start_kind character(1) NOT NULL DEFAULT 't'::bpchar,--by time or after previous valve stop or manual ('t' || 'p' || 'm')
  month_plan character(12) NOT NULL DEFAULT '111111111111'::bpchar,-- jan-dec
  weekday_plan character(7) NOT NULL DEFAULT '1111111'::bpchar,-- sunday-saturday
  time_plan_id integer NOT NULL DEFAULT -1,
  change_plan_id integer NOT NULL DEFAULT -1,
  CONSTRAINT prog_pkey PRIMARY KEY (id),
  CONSTRAINT prog_start_kind_check CHECK (start_kind = 't'::bpchar OR start_kind = 'p'::bpchar OR start_kind = 'm'::bpchar)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.valve
(
  id integer NOT NULL,
  name character varying(32) NOT NULL,
  prev_id integer NOT NULL DEFAULT -1,--previous valve for prog.sstart_kind
  prog_id integer NOT NULL DEFAULT -1,
  is_master integer  NOT NULL DEFAULT 0,
  master_id integer NOT NULL DEFAULT -1,
  rain_sensitive integer NOT NULL DEFAULT 0,
  rain_sensor_id integer NOT NULL DEFAULT -1,
  em_id integer NOT NULL DEFAULT -1,
  CONSTRAINT valve_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.time_plan
(
  id integer NOT NULL,
  start_time integer NOT NULL,
  CONSTRAINT time_plan_pkey PRIMARY KEY (id, start_time),
  CONSTRAINT time_plan_start_time_check CHECK (start_time >= 0 AND start_time < 86400)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.change_plan
(
  id integer NOT NULL,
  seq integer NOT NULL,
  gap interval NOT NULL DEFAULT '0',
  shift integer NOT NULL DEFAULT 0,
  CONSTRAINT change_plan_pkey PRIMARY KEY (id, seq)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.lock_em
(
  em_id integer NOT NULL,
  CONSTRAINT lock_em_pkey PRIMARY KEY (em_id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.sensor_mapping
(
  app_class character varying(32) NOT NULL,
  sensor_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  remote_id integer NOT NULL,
  CONSTRAINT sensor_mapping_pkey PRIMARY KEY (app_class, sensor_id, peer_id, remote_id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE irg.em_mapping
(
  app_class character varying(32) NOT NULL,
  em_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  remote_id integer NOT NULL,
  CONSTRAINT em_mapping_pkey PRIMARY KEY (app_class, em_id, peer_id, remote_id)
)
WITH (
  OIDS=FALSE
);

CREATE OR REPLACE FUNCTION irg.add_prog()
  RETURNS integer AS
$BODY$declare
 n integer;
 prg_id integer;
 prg_nid integer;
 prg_name varchar(16);
begin
 n=-1;
 prg_id=-1;
 while n<>0 and prg_id<65535 loop
  prg_id=prg_id+1;
  select count(*) from irg.prog where id=prg_id into n; 
  if not FOUND then
   raise exception 'id loop failed when prg_id was: % ', prg_id;
  end if;
 end loop;
 n=-1;
 prg_nid=-1;
 while n<>0 and prg_nid<65535 loop
  prg_nid=prg_nid+1;
  prg_name='ПРОГРАММА ' || prg_nid;
  select count(*) from irg.prog where name=prg_name into n; 
  if not FOUND then
   raise exception 'name loop failed when prg_nid was: % ', prg_nid;
  end if;
 end loop;
 insert into irg.prog(id, name) values (prg_id, prg_name);
 if not FOUND then
   raise exception 'insert failed when prg_id was: % ', prg_id;
 end if;
 return prg_id;
end;$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION irg.add_tp(tp_id integer, tp_time integer)
  RETURNS integer AS
$BODY$declare
 new_time integer;
begin
 new_time=tp_time+1;
 if new_time>=0 and new_time<86400 then
  insert into irg.time_plan(id, start_time) values (tp_id, new_time);
  if not FOUND then
   raise exception 'add_tp: insert failed';
  end if;
 else
  raise exception 'add_tp: bad start_time';
 end if;
 return new_time;
end;$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION irg.add_cp(cp_id integer)
  RETURNS integer AS
$BODY$declare
 n integer;
 new_seq integer;
begin
 select max(seq) from irg.change_plan where id=cp_id into n;
 if not FOUND then
  new_seq=0;
 else
  if n is null then
   n=-1;
  end if;
  new_seq=n+1;
  if new_seq<=n then
   raise exception 'add_cp: bad new_seq';
  end if;
 end if;
 insert into irg.change_plan(id, seq) values (cp_id, new_seq);
 if not FOUND then
  raise exception 'add_tp: insert failed';
 end if;
 return new_seq;
end;$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

