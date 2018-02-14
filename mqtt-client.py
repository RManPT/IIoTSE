### Taken from https://pypi.python.org/pypi/paho-mqtt
### Requires Paho-MQTT package, install by:
### pip install paho-mqtt
import ssl
import paho.mqtt.client as mqtt

MQTT_KEEPALIVE    = 60
MQTT_URL          = "things.ubidots.com"
MQTT_PORT         = 8883
MQTT_TOPIC    = "/v1.6/devices/remote1/avac"
user = "A1E-zitKYN4n3BzC0Dx8mJsFymGqmrG0Mg"
password = ""

def on_connect(client, userdata, flags, rc):
    if rc == 0:
	    print("Connected with result code " + str(rc))
	    print("Subscribed to " + MQTT_TOPIC)
	    client.subscribe(MQTT_TOPIC)
    else:
	    print("Connection failed")
	    
# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))

client = mqtt.Client()
client.username_pw_set(user, password=password)
client.on_connect = on_connect
client.on_message = on_message

print("connecting to " + MQTT_URL)
client.tls_set(ca_certs="industrial.pem", certfile=None, keyfile=None,cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
client.tls_insecure_set(False)
client.connect(MQTT_URL, MQTT_PORT, MQTT_KEEPALIVE)
client.loop_forever()
