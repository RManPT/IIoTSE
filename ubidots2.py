import csv
import sys
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
MQTT_URL_TOPIC    = "/v1.6/devices/remote1/avac"
user = "A1E-zitKYN4n3BzC0Dx8mJsFymGqmrG0Mg"
password = ""
Connected = False


#------------------------------------------------------------#
# Helper functions
#------------------------------------------------------------#

# -----------------------------------------------------------#
# MQTT related functions
# -----------------------------------------------------------#
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        client.subscribe(MQTT_URL_TOPIC)
        print("Subscribed to " + MQTT_URL_TOPIC)
        global Connected                #Use global variable
        Connected = True                #Signal connection 
    else:
        print("Connection failed")
#------------------------------------------------------------#
def on_message(client, userdata, msg):
  print("MQTT: RX: " + msg.topic + " : " + str(msg.payload))

#------------------------------------------------------------#
# UDP6 and MQTT client session
#------------------------------------------------------------#

 
client = mqtt.Client()
client.username_pw_set(user, password=password)
client.on_connect = on_connect
client.on_message = on_message

client.tls_set(ca_certs="industrial.pem", certfile=None, keyfile=None,cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
client.tls_insecure_set(False)
client.connect(MQTT_URL, MQTT_PORT, MQTT_KEEPALIVE)
client.loop_forever()
  
  

