import paho.mqtt.client as mqttClient
import time
import json
import ssl

def on_connect(client, userdata, flags, rc):
 
    if rc == 0:
 
        print("Connected to broker")
 
        global Connected                #Use global variable
        Connected = True                #Signal connection 
 
    else:
 
        print("Connection failed")

def on_publish(client, userdata, result):

	print("Published!")


Connected = False
broker_address= "things.ubidots.com"
port = 8883
user = "A1E-zitKYN4n3BzC0Dx8mJsFymGqmrG0Mg"
password = ""
topic = "/v1.6/devices/remote1"

client = mqttClient.Client()
client.username_pw_set(user, password=password)
client.on_connect = on_connect
client.on_publish = on_publish
client.tls_set(ca_certs="industrial.pem", certfile=None, keyfile=None,cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
client.tls_insecure_set(False)
client.connect(broker_address, port=port)
client.loop_start()

while Connected != True:    #Wait for connection
    print("Connecting...")
    time.sleep(1)
 
try:
    while True:
        payload = json.dumps({"Battery":2220})
        print(payload)
        client.publish(topic, payload)
        time.sleep(10)
 
except KeyboardInterrupt:
 
    client.disconnect()
    client.loop_stop()
