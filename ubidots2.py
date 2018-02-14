import csv
import sys, os
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
MQTT_URL_TOPIC    = "/v1.6/devices/remote1/smart"

user = "A1E-zitKYN4n3BzC0Dx8mJsFymGqmrG0Mg"
password = ""
Connected = False
devices = []
DEVICES_FILE = "devices.txt"

#------------------------------------------------------------#
# Helper functions
#------------------------------------------------------------#

# -----------------------------------------------------------#
# MQTT related functions
# -----------------------------------------------------------#
#------------------------------------------------------------#
def getstatus(msg):
	value = int(float(str(msg.payload).split("'")[1]))
	if value:
		value = "On"
	else:
		value = "Off"
	return value
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
#------------------------------------------------------------#
def on_connect(client, userdata, flags, rc):
	if rc == 0:
		print("Connected to broker")
		client.subscribe("/v1.6/devices/remote1/smart/lv")
		for dev in devices:
			client.subscribe("/v1.6/devices/remote" + dev + "/avac/lv")
			client.subscribe("/v1.6/devices/remote" + dev + "/light/lv")
		print("Subscribed to selected topics")
		global Connected                #Use global variable
		Connected = True                #Signal connection 
	else:
		print("Connection failed")
#------------------------------------------------------------#
def on_message(client, userdata, msg):
	#print("MQTT: RX: " + msg.topic + " : " + str(msg.payload))
	print(str(datetime.datetime.now()) + " : ", end = "")
	topic = msg.topic.rsplit('/')[-2].upper()
	area = msg.topic.rsplit('/')[-3].replace("remote","area ").upper()
	value = getstatus(msg)
	if topic == "SMART":
		print("\n\n------------------------------------------")
		print("MQTT: " + topic + " system is now " + value)
		if value.upper()=="OFF":
			print("(User options only)")
		else:
			print("(System options will override user's)")
		print("------------------------------------------\n\n")
	else: 
		print("MQTT: Turning '" + area + "' " + topic + " " + value + " > ", end = " ")
		#sent = sock.sendto(message.encode(), multicast_group)
		print("(order sent)")
#------------------------------------------------------------#
# UDP6 and MQTT client session
#------------------------------------------------------------#


#message = "teste"
#multicast_group = ('fd00::1', PORT+1)
#sock = socket(AF_INET6, SOCK_DGRAM)
#sock.settimeout(0.2)
#ttl = struct.pack('b',1)
#sock.setsockopt(IPPROTO_IPV6, IPV6_MULTICAST_HOPS, ttl)


getdevices() 
client = mqtt.Client()
client.username_pw_set(user, password=password)
client.on_connect = on_connect
client.on_message = on_message

client.tls_set(ca_certs="industrial.pem", certfile=None, keyfile=None,cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
client.tls_insecure_set(False)
client.connect(MQTT_URL, MQTT_PORT, MQTT_KEEPALIVE)
client.loop_forever()
  
  

