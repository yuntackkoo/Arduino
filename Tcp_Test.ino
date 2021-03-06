#include <Adafruit_CC3000.h>
#include <Adafruit_CC3000_Server.h>
#include <ccspi.h>
#include <SPI.h>
#include <Servo.h>

#define ADAFRUIT_CC3000_IRQ 7 //인터럽트 컨트롤 핀, Wido 보드 사용시 7로 변경.
#define ADAFRUIT_CC3000_VBAT 5
#define ADAFRUIT_CC3000_CS 10
#define MAX_LENGTH 33
#define PORT 255
#define TOTALL_PACK 33    //총 패킷 사이즈 33Byte
#define USER_SIZE 4 //최대 연결 숫자

const byte Re_Request = 0;//재요청
const byte Join = 1;//접속
const byte Key_Ex = 2;//키교환
const byte UnLock = 3;//잠금해제
const byte Req_Data = 4;//로그 요청
const byte Req_KeyOffer = 5;//추가 등록시 키 요청
const byte Confirm_KeyOffer = 6;//추가 등록시 키 확인

const byte Response = 60;//응답
const byte Confirm_KeyEx = 61;//초기 등록 또는 추가 등록시 키교환 확인
const byte KeyOffer = 62;//키 교환 요구 받을시 키 제공
const byte Offer_Data = 63;//로그 요청 받을시 로그 응답
const byte Unlock_Other = 64;//다른 사람이 문 열때 서버에서 전송

int servoPin = 12;
Servo servo;
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);
Adafruit_CC3000_Server server = Adafruit_CC3000_Server(PORT);

String SSID;
String PASS;

#define WLAN_SECURITY WLAN_SEC_UNSEC

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

long seq_num_arr[USER_SIZE];

uint32_t ip = 0;

void setup() {
	Serial.begin(9600);
	servo.attach(servoPin);
	servo.write(0);
	delay(250);
	servo.detach();
	SSID = String("AndroidHotspot6558");
	PASS = String("flyiceball!");

	pinMode(LED_BUILTIN, OUTPUT);
	//Wi-Fi 연결 시도
	if (!cc3000.begin())
	{
		Serial.println(F("Couldn't begin()! Check your wiring?"));
		while (1);
	}


	wifiScan();
	joinWiFi();

	server.begin();
	servo.write(0);
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

	SwitchPacket();
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
		Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ip);
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
	*pointer++ = packet[offset++]; //code를 복사
	*pointer++ = packet[offset++]; //패딩사이즈를 복사
	offset += recpacket.paddingSize; //패딩부분을 지나침
	pointer += recpacket.paddingSize; //패딩부분을 지나침
	recpacket.datasize = TOTALL_PACK - 7 - recpacket.paddingSize;
	for (int i = offset; i<offset + 4; i++) {
		*pointer++ = packet[i];//패딩부분 이후 나머지 데이터를 복사
	}

	offset += 5;

	for (int i = 0; i < recpacket.datasize; i++) {
		recpacket.data[i] = packet[offset++];
	}
	
}

void SwitchPacket() {
	int number = server.availableIndex();
	if (number >= 0) {
		Adafruit_CC3000_ClientRef client = server.getClientRef(number);
		client.readBytes(packetBuffer, TOTALL_PACK);
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

		switch (recpacket.code)
		{
		case Join:
			Serial.println("\n\nStart Send Packet\n");
			sendpacket.code = Response;
			sendpacket.seqNum = random();
			seq_num_arr[number] = sendpacket.seqNum;
			FillSendBuffer(0);
			client.write(sendBuffer, TOTALL_PACK);
			Serial.println("Complete Send Packet");
			Serial.print("Send Seq_Num : ");
			Serial.println(sendpacket.seqNum);
			break;

		case Key_Ex:
			Serial.println("\n\nStart Send Packet\n");
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
			sendpacket.data[4] = recpacket.data[4];
			sendpacket.data[5] = recpacket.data[5];
			sendpacket.data[6] = recpacket.data[6];
			sendpacket.data[7] = recpacket.data[7];
			FillSendBuffer(8);
			servo.attach(servoPin);
			servo.write(80);
			server.write(sendBuffer, TOTALL_PACK);
			delay(250);
			servo.write(0);
			delay(250);
			servo.detach();
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