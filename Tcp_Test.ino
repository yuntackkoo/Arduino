#include <Adafruit_CC3000.h>
#include <Adafruit_CC3000_Server.h>
#include <ccspi.h>
#include <SPI.h>
#include <Adafruit_CC3000_Library-master\utility\socket.h>

#define ADAFRUIT_CC3000_IRQ 7 //���ͷ�Ʈ ��Ʈ�� ��, Wido ���� ���� 7�� ����.
#define ADAFRUIT_CC3000_VBAT 5
#define ADAFRUIT_CC3000_CS 10
#define MAX_LENGTH 33
#define PORT 255
#define TOTALL_PACK 33    //�� ��Ŷ ������ 33Byte


const byte Re_Request = 0;//���û
const byte Join = 1;//����
const byte Key_Ex = 2;//Ű��ȯ
const byte UnLock = 3;//�������
const byte Req_Data = 4;//�α� ��û
const byte Req_KeyOffer = 5;//�߰� ��Ͻ� Ű ��û
const byte Confirm_KeyOffer = 6;//�߰� ��Ͻ� Ű Ȯ��

const byte Response = 60;//����
const byte Confirm_KeyEx = 61;//�ʱ� ��� �Ǵ� �߰� ��Ͻ� Ű��ȯ Ȯ��
const byte KeyOffer = 62;//Ű ��ȯ �䱸 ������ Ű ����
const byte Offer_Data = 63;//�α� ��û ������ �α� ����
const byte Unlock_Other = 64;//�ٸ� ����� �� ���� �������� ����


Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);

String SSID;
String PASS;

#define WLAN_SECURITY WLAN_SEC_WPA2

typedef struct _packet {
	byte datasize;
	byte code;
	byte paddingSize;
	byte id;
	long seqNum;
	byte data[30];
} Packet;

byte packetBuffer[TOTALL_PACK];
byte sendBuffer[TOTALL_PACK];
Packet recpacket;
Packet sendpacket;
int sock;
sockaddr_in server;
uint32_t ip = 0;

void setup() {
	Serial.begin(9600);
	uint32_t tmp;
	SSID = String("iptimer");
	PASS = String("flyiceball!");

	pinMode(LED_BUILTIN, OUTPUT);
	//Wi-Fi ���� �õ�
	if (!cc3000.begin())
	{
		Serial.println(F("Couldn't begin()! Check your wiring?"));
		while (1);
	}

	wifiScan();
	joinWiFi();
	

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = ip;

	bind(sock, (sockaddr *)&server, sizeof(server));
	
}

void loop() {
	if (cc3000.getStatus() != STATUS_CONNECTED) {
		Serial.println();
		Serial.println("WiFi Connect Lost ReConnect!");
		Serial.println();
		
		digitalWrite(LED_BUILTIN, LOW);

		cc3000.reboot();

		joinWiFi();
	}
	delay(1000);
}

bool displayConnectionDetails(void)
{
	uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

	if (!cc3000.getIPAddress(&ip, &netmask, &gateway, &dhcpserv, &dnsserv))
	{
		Serial.println(F("Unable to retrieve the IP Address!\r\n"));
		return false;
	}
	else
	{
		Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
		Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
		Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
		Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
		Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
		Serial.println();
		return true;
	}
}

bool joinWiFi(void) {
	Serial.print(F("\nAttempting to connect to "));
	Serial.println(SSID);
	char *CSSID = (char *)malloc((SSID.length()) * sizeof(char));
	char *CPASS = (char *)malloc((PASS.length()) * sizeof(char));

	SSID.toCharArray(CSSID, SSID.length()+1);
	PASS.toCharArray(CPASS, PASS.length()+1);

	if (!cc3000.connectToAP(CSSID, CPASS, WLAN_SECURITY)) {
		Serial.println(F("Failed!"));
		return false;
	}

	Serial.println(F("Connected!"));

	Serial.println(F("Request DHCP"));
	while (!cc3000.checkDHCP())
	{
		delay(100); // ToDo: Insert a DHCP timeout!
	}

	while (!displayConnectionDetails()) {
		delay(1000);
	}

	digitalWrite(LED_BUILTIN, HIGH);

	free(CSSID);
	free(CPASS);

	return true;
}

void wifiScan(void)
{
	uint8_t valid, rssi, sec;
	uint32_t index;
	uint32_t index2;
	char ssidname[33];
	cc3000.startSSIDscan(&index);
	Serial.print(("Networks found : "));
	Serial.println(index);
	Serial.println((" == == == == == == == == == == == == == == == == == == == == == == == == "));
	while (index) {
		index--;
		valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
		Serial.print(("SSID Name : "));
		Serial.print(ssidname);
		Serial.println();
		Serial.print(("RSSI : "));
		Serial.println(rssi);
		Serial.print(("Security Mode : "));
		Serial.println(sec);
		Serial.println();
	}
	Serial.println((" == == == == == == == == == == == == == == == == == == == == == == == == "));
	cc3000.stopSSIDscan();
}

void FillSendBuffer(int datasize) {
	int offset = 0;
	sendBuffer[offset++] = sendpacket.code;
	sendpacket.paddingSize = TOTALL_PACK - datasize - 7;
	sendpacket.datasize = 26 - sendpacket.paddingSize;
	sendBuffer[offset++] = sendpacket.paddingSize;
	for (; offset < sendpacket.paddingSize + 2; offset++) {
		sendBuffer[offset] = rand();
	}
	offset++;
	byte *p = (byte *)&sendpacket.seqNum;
	for (int i = 0; i < datasize + 5; i++) {
		sendBuffer[offset++] = *p++;
	}
}

void Parsing(byte *packet) {
	int offset = 0;
	byte *pointer = (byte *)&recpacket.code;
	*pointer++ = packet[offset++]; //code�� ����
	*pointer++ = packet[offset++]; //�е������ ����
	offset += recpacket.paddingSize; //�е��κ��� ����ħ
	pointer += recpacket.paddingSize; //�е��κ��� ����ħ
	recpacket.datasize = TOTALL_PACK - 7 - recpacket.paddingSize;
	for (int i = offset; i<offset + 4; i++) {
		*pointer++ = packet[i];//�е��κ� ���� ������ �����͸� ����
	}

	offset += 5;

	for (int i = 0; i < recpacket.datasize; i++) {
		recpacket.data[i] = packet[offset++];
	}
}

void SwitchPacket(int number) {
	if (client[number].connected()) {

		int i = 0;

		if (!client[number].connected()) {
			Serial.println("New client connection.");
		}

		if (client[number].available()) {
			client[number].readBytes(packetBuffer, TOTALL_PACK);
			Parsing(packetBuffer);
			int _code = to_int(recpacket.code);
			int _id = to_int(recpacket.id);
			Serial.print("Code : ");
			Serial.println(_code);
			Serial.print("Padding Size : ");
			Serial.println(recpacket.paddingSize);
			Serial.print("SeqNum : ");
			Serial.println(recpacket.seqNum, DEC);
			Serial.print("ID : ");
			Serial.println(_id);
		}

		switch (recpacket.code)
		{
		case Join:
			Serial.println("\n\nStart Send Packet\n");
			sendpacket.code = Response;
			
			if (recpacket.id < 0) {
				sendpacket.id = recpacket.id;
			}
			else
			{
				sendpacket.id = cuid++;
			}
			sendpacket.seqNum = random();
			seq_num_arr[number] = sendpacket.seqNum;
			FillSendBuffer(0);
			client[number].write(sendBuffer, TOTALL_PACK);
			Serial.println("Complete Send Packet");
			Serial.print("Send Seq_Num : ");
			Serial.println(sendpacket.seqNum);
			break;

		case Key_Ex:
			Serial.println("\n\nStart Send Packet\n");
			usertable[recpacket.id] = String((char *)recpacket.data);
			Serial.println(usertable[recpacket.id]);
			Serial.println(recpacket.datasize);
			Serial.println("\nStart Show Data");
			for (int i = 0; i < recpacket.datasize; i++) {
				Serial.print((char)recpacket.data[i]);
			}
			break;

		case UnLock:
			Serial.println("\n\nStart Send Packet\n");
			sendpacket.code = Unlock_Other;
			sendpacket.data[0] = recpacket.data[0];
			sendpacket.data[1] = recpacket.data[1];
			sendpacket.data[2] = recpacket.data[2];
			sendpacket.data[3] = recpacket.data[3];
			sendpacket.data[4] = recpacket.id;
			FillSendBuffer(5);
			for (int i = 0; i < 5; i++) {
				if (client[i].connected()) {
					if (i != number) {
						sendpacket.seqNum = seq_num_arr[i];
						client[i].write(sendBuffer, TOTALL_PACK);
					}
				}
			}
			break;
		default:
			break;
		}
		initPacket();
	}
}


int to_int(byte b) {
	return (b & 0xFFFF);
}

void initPacket() {
	recpacket.code = -1;

}