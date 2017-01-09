import collections
import signal
import struct
import multiprocessing as mp
from influxdb import InfluxDBClient

import os
pid = str(os.getpid())
pidfile = "/tmp/logger.pid"
if os.path.isfile(pidfile):
    os.remove(pidfile)
file(pidfile, 'w').write(pid)

def receive_signal(signum, stack):
    global state
    if signum == signal.SIGUSR1:
        state=0
    if signum == signal.SIGUSR2:
        state=1

signal.signal(signal.SIGUSR1, receive_signal)
signal.signal(signal.SIGUSR2, receive_signal)

R1=10.000
R2=46.400
RSENSE=0.510
VREF=2.048
CODE_MAX=4096
GAIN=25

USER = 'root'
PASSWORD = 'root'
DBNAME = 'testdb'
HOST = 'iti-pc-67.tugraz.at'
PORT = '8086'

AVERAGE = 5000

points=[]

client = InfluxDBClient(HOST, PORT, USER, PASSWORD, DBNAME)
#client.drop_database(DBNAME)
#client.create_database(DBNAME)
client.switch_database(DBNAME)

voltage_scale=(R1+R2)/(R1)*VREF/CODE_MAX;
current_scale=VREF/(RSENSE*CODE_MAX*GAIN)*1000.0;

Timestamp = collections.namedtuple('Timestamp', 'ts tns')
Reading = collections.namedtuple('Reading', 'current voltage gpio')

avg_voltage=0
avg_current=0
avg_energy=0.0
avg_count=0
start_ts=0
last_ts=-1
latch_gpio=0
latch_reset=0

state=0

int_energy=0.0

def upload_results(avg_count,avg_voltage,avg_current,avg_energy,latch_gpio,latch_reset,int_energy,state):
    point = {
        "measurement": 'N40',
        "time": int(avg_ts),
        "fields": {
            "voltage": avg_voltage/avg_count,
            "current": avg_current/avg_count,
            "energy": avg_energy/avg_count*1000,
            "gpio": latch_gpio,
            "reset": latch_reset,
            "int_energy": int_energy,
            "state": state
        }
    }   
 
    client.write_points([point],time_precision='u')


f = open('/fifo/logger')
print("connected to fifo: " + f.name )

while True:
    msg=""
    while not (len(msg) == 13):
        msg=msg+f.read(13-len(msg))
    r = Reading._make(struct.unpack("<HHB",msg[:5]))
    t = Timestamp._make(struct.unpack("<LL",msg[5:]))
    voltage=voltage_scale*r.voltage;
    current=current_scale*r.current;

    current_ts=t.ts*1000000+t.tns/1000

    if not last_ts == -1:
        energy=voltage*current/1000.0*((current_ts-last_ts)/1000000.0)
    else:
        energy=0.0

    if state == 0:
        int_energy=0.0
    if state == 1:
        int_energy=int_energy+energy

    last_ts=current_ts;
 
    if avg_count==0:
        start_ts=current_ts

    avg_current=avg_current+current
    avg_voltage=avg_voltage+voltage
    avg_energy=avg_energy+energy
    avg_count=avg_count+1

    if (r.gpio&0x01) == 0:
        latch_gpio=1
    if (r.gpio&0x02) == 2:
        latch_reset=1
    if (r.gpio&0x04) == 4:
        latch_reset=1
        if state == 0:
            state = 1
        elif state == 1:
            state = 0

    if avg_count==AVERAGE:
        avg_ts=(start_ts+current_ts)/2
 #      upload_results(avg_count,avg_voltage,avg_current,avg_energy,latch_gpio,latch_reset,int_energy,state)
        p = mp.Process(target=upload_results,args=(avg_count,avg_voltage,avg_current,avg_energy,latch_gpio,latch_reset,int_energy,state))
        p.start()
        avg_count=0
        avg_voltage=0
        avg_energy=0.0
        avg_current=0
        latch_reset=0
        latch_gpio=0

