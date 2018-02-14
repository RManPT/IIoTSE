import csv
import sys, os.path
import json
import datetime
from socket import*
from socket import error
from time import sleep
import struct
from ctypes import *
import time
import ssl
import paho.mqtt.client as mqtt
#------------------------------------------------------------#
ID_STRING      = "V0.1"
#------------------------------------------------------------#
PORT              = 5678
CMD_PORT          = 8765
BUFSIZE           = 1024
#------------------------------------------------------------#
MQTT_KEEPALIVE    = 60
MQTT_URL          = "things.ubidots.com"
MQTT_PORT         = 8883
MQTT_URL_TOPIC    = "/v1.6/devices/"
user = "A1E-zitKYN4n3BzC0Dx8mJsFymGqmrG0Mg"
password = ""
Connected = False
DEVICES_FILE = "devices.txt"
devices = []
SMART = 1

#------------------------------------------------------------#
# Message structure
#------------------------------------------------------------#
var1 = "core_temp"
var2 = "Temperature"
var3 = "Humidity"
var4 = "Luminosity"

class SENSOR(Structure):
    _pack_   = 1
    _fields_ = [
                 ("id",                         c_uint8),
                 ("Light",                      c_bool),
                 ("AVAC",                       c_bool),
                 (var1,                         c_uint16),
                 (var2,                         c_uint16),
                 (var3,                         c_uint16),
                 (var4,                         c_uint16),
                 ("Battery",                    c_uint16)
               ]

    def __new__(self, socket_buffer):
        return self.from_buffer_copy(socket_buffer)

    def __init__(self, socket_buffer):
        pass
#------------------------------------------------------------#
# Helper functions
#------------------------------------------------------------#

def getstatus(msg):
	value = int(float(str(msg.payload).split("'")[1]))
	if value:
		value = "On"
	else:
		value = "Off"
	return value
#------------------------------------------------------------#
def print_recv_data(msg):
  print("***")
  for f_name, f_type in msg._fields_:
    print("{0}:{1} ".format(f_name, getattr(msg, f_name))),
  print
  print("***")
#------------------------------------------------------------# 
def save_recv_data(msg):
   sensordata = str(datetime.datetime.now()) + ','
   for f_name, f_type in msg._fields_:
       if (str(f_name)=="Light" or str(f_name)=="AVAC"):
         sensordata += str(int(getattr(msg, f_name)))  + ','
       elif (str(f_name)=="core_temp"):  
         sensordata += str(getattr(msg, f_name)//1000)  + '.' + str(getattr(msg, f_name)%1000) + ','
       elif (str(f_name)=="Temperature" or str(f_name)=="Humidity"):  
         sensordata += str(getattr(msg, f_name)//10)  + '.' + str(getattr(msg, f_name)%10) + ','     
       else:  
         sensordata += str(getattr(msg, f_name)) + ','                   
       
       
   sensordata = sensordata[:-1]
   
   x = (sensordata.split(","))
   
   print(x)
   with open(sys.argv[1], "a", newline="") as fdata:
     fhandler = csv.writer(fdata,quoting=csv.QUOTE_ALL)
     fhandler.writerow(x)
     fdata.close()   
#------------------------------------------------------------#
def publish_recv_data(data, pubid, conn, addr):
  try:
    conn.publish(MQTT_URL_TOPIC+"remote"+str(pubid), payload=data)
    print("MQTT: Published")
  except Exception as error:
    print(error)
#------------------------------------------------------------#
def adddevice(dev):
	seen = set()

	if os.path.isfile(DEVICES_FILE)==0:
		with open(DEVICES_FILE, "w", newline="") as fhand:
			fhand.write(dev)
			fhand.close()
	else:
		with open(DEVICES_FILE, "r+", newline="") as fhand:
			for line in fhand:
				devices = line.split()
				for device in devices:
					if device not in seen:
						seen.add(device)
			if dev not in seen:
				fhand.write(dev)
				fhand.close()
#------------------------------------------------------------#
def getdevices():
	seen = set()
	global devices
	if os.path.isfile(DEVICES_FILE):
		with open(DEVICES_FILE, "r", newline="") as fhand:
			for line in fhand:
				devs = line.split()
				for device in devs:
					if device not in seen:
						seen.add(device)
						devices.append(device)	
# -----------------------------------------------------------#
def jsonify_recv_data(msg):
   sensordata = "{"
   for f_name, f_type in msg._fields_:
     if (str(f_name)!="id"):
       if (str(f_name)=="Light" or str(f_name)=="AVAC"):
         if (SMART==1):
           sensordata += '"' + str(f_name) + '":' + str(int(getattr(msg, f_name)))  + ','
       elif (str(f_name)=="core_temp"):  
         sensordata += '"' + str(f_name) + '":' + str(getattr(msg, f_name)//1000)  + '.' + str(getattr(msg, f_name)%1000) + ','
       elif (str(f_name)=="Temperature" or str(f_name)=="Humidity"):  
         sensordata += '"' + str(f_name) + '":' + str(getattr(msg, f_name)//10)  + '.' + str(getattr(msg, f_name)%10) + ','     
       else:  
         sensordata += '"' + str(f_name) + '":' + str(getattr(msg, f_name)) + ','              
     else:
       adddevice(str(getattr(msg, f_name)))     

   sensordata = sensordata[:-1]
   sensordata += "}"
   print(sensordata)
   return sensordata
# -----------------------------------------------------------#
def send_udp_cmd(addr):
  client = socket(AF_INET6, SOCK_DGRAM)
  print("Sending reply to " + addr)

  #try:
    #client.sendto("Hello from the server", (addr, CMD_PORT))
  #except Exception as error:
    #print(error)

  client.close()
# -----------------------------------------------------------#
# MQTT related functions
# -----------------------------------------------------------#
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        client.subscribe(MQTT_URL_TOPIC + "remote1/smart/lv")
        global Connected                #Use global variable
        Connected = True                #Signal connection 
    else:
        print("Connection failed")
#------------------------------------------------------------#
def on_message(client, userdata, msg):
	#print("MQTT: RX: " + msg.topic + " : " + str(msg.payload))
	topic = msg.topic.rsplit('/')[-2].upper()
	

	global SMART
	if (topic == "SMART"):
		value = getstatus(msg)
		print("\n\n------------------------------------------")
		print("MQTT: SMART system is now " + value)
		if value.upper()=="OFF":
			print("(User options only)")
			SMART=0
		else:
			print("(System options will override user's)")
			SMART=1
		print("------------------------------------------\n\n")
		print("\nWaiting for data...")

	
#------------------------------------------------------------#
def on_publish(client, packet, mid):
  print("MQTT: Published {0}".format(mid))
#------------------------------------------------------------#
# UDP6 and MQTT client session
#------------------------------------------------------------#
def start_client():
  try:
    os.remove(DEVICES_FILE)
  except:
    print(DEVICES_FILE + " not found!")
  now = datetime.datetime.now()
  print("UDP6-MQTT server side application "  + ID_STRING)
  print("Started " + str(now))
  try:
    s = socket(AF_INET6, SOCK_DGRAM)
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

    # Replace address below with "aaaa::1" if tunslip6 has created a tunnel
    # interface with this address
    s.bind(('fd00::1', PORT))

  except Exception:
    print("ERROR: Server Port Binding Failed")
    return
  print('UDP6-MQTT server ready: %s'% PORT)
  print("msg structure size: ", sizeof(SENSOR))
  





  client = mqtt.Client()
  client.username_pw_set(user, password=password)
  client.on_connect = on_connect
  client.on_message = on_message
  client.on_publish = on_publish
  client.tls_set(ca_certs="industrial.pem", certfile=None, keyfile=None,cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
  client.tls_insecure_set(False)
  client.connect(MQTT_URL, port=MQTT_PORT) 
  
  client.loop_start()
  # Start the MQTT thread and handle reconnections, also ensures the callbacks
  # being triggered




  while Connected != True:    #Wait for connection
    print("Connecting...")
    time.sleep(1)
    
  while True:
    print("\n\n------------------------------------------")
    print("Waiting for data...")
    data, addr = s.recvfrom(BUFSIZE)
    now = datetime.datetime.now()
    print(str(now)[:19] + " -> " + str(addr[0]) + ":" + str(addr[1]) + " " + str(len(data)))
    msg_recv = SENSOR(data)
    print("\nParsing data...")
    #print_recv_data(msg_recv)
    sensordata = jsonify_recv_data(msg_recv)
    print("\nStoring localy...")
    save_recv_data(msg_recv)    
    print("\nPublishing...")
    publish_recv_data(sensordata, msg_recv.id, client, addr[0])	 
    print("\nAcknowledging sender...")
    send_udp_cmd(addr[0])
    print("------------------------------------------\n\n")
    time.sleep(5)
    #d = s.send("teste".encode(),addr)


  client.loop_stop()
  client.disconnect()

#------------------------------------------------------------#
# MAIN APP
#------------------------------------------------------------#
if __name__=="__main__":
  start_client()

