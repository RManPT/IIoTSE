IIoTSE

Certificar que tens o python3 instalado assim como o paho-mqtt (instala o pip3 antes)


TERM1
 make border-router.upload MOTES=/dev/ttyUSB1 && make connect-router PREFIX="-s /dev/ttyUSB1 fd00::1/64"
 
	Inicializa Edge Router com 6loWPAN e RPL fazendo um tunel SLIP com a porta USB
	Activa rede sub-1Ghz para maior range com menos potencia e maior capacidade de contornar obstaculos

TERM2
 make iotnode.upload MOTES=/dev/ttyUSB0 && make login MOTES=/dev/ttyUSB0
 
	Carrega o mote com muitas funcionalidades:
		- Leitura de sensores internos
		- Leitura de sensores externos
		- Comunicao de leituras apenas quando sofrem alterações ou por intervencao de utilziador para o edge router
		- Activa/desactiva actuadores mediante thresholds
		- Permite visualizar em color code o estado dos sensores (pode ficar off para poupar energia)
		x Pode receber mensagens para activar/desactivar actuadores remotamente (desactivado por opção)
		x Pode receber mensagens para fazer mesh repeater (desactivado por opção)
 
TERM3
 sudo python3 ubidots3.py data.csv
 
	Servidor de MQTT que captura todos os dados que chegam ao ER e comunicam devidamente formatados de forma segura (TLS)
	para a api da UBIDOTS
	
TERM4
 sudo python3 ubidots2.py
	captura de dados da ubidots para enviar para a rede as opcoes de activar/desactivar actuadores
	(registado ao avac para testes ja detecta quando se pressiona o switch avac)

DASHBOARD
	https://app.ubidots.com/ubi/public/getdashboard/page/tUmsg8fMeDg3sYIfSzdQZ0kl7Xw
	
Os ficheiros na pasta EXTRA são ficheiros do contiki que tiveram de ser alterados:
	buttons-sensors.h deve ser colocado na pasta /contiki/platform/zoul/dev
	Makefile.C2538 deve ser colocado na pasta /contiki/cpu/cc2538