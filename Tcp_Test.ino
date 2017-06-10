#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <time.h>
#include <Servo.h>

#define PORT 255
#define TOTALL_PACK 33    //총 패킷 사이즈 33Byte

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


YunServer server(PORT);
YunClient client;

static boolean clientActive = false;
char SerialChat;

typedef struct _packet {
	byte code;
	byte paddingSize;
	long seqNum;
	byte id;
	byte data[30];
} Packet;

byte packetBuffer[TOTALL_PACK];
byte sendBuffer[TOTALL_PACK];
Packet recpacket;
Packet sendpacket;
Servo servo;
int Angle;

void Parsing(byte *packet) {
	int offset = 0;
	byte *pointer = (byte *)&recpacket;
	*pointer++ = packet[offset++]; //code를 복사
	*pointer++ = packet[offset++]; //패딩사이즈를 복사
	offset += recpacket.paddingSize; //패딩부분을 지나침
	pointer += recpacket.paddingSize; //패딩부분을 지나침
	for (int i = offset; i<TOTALL_PACK; i++) {
		*pointer++ = packet[offset];//패딩부분 이후 나머지 데이터를 복사
	}
}

void FillSendBuffer(int datasize) {
	int offset = 0;
	sendBuffer[offset++] = sendpacket.code;
	sendpacket.paddingSize = TOTALL_PACK - datasize - 7;
	sendBuffer[offset++] = sendpacket.paddingSize;
	for (; offset < sendpacket.paddingSize+2; offset++) {
		sendBuffer[offset] = rand();
	}
	offset++;
	byte *p = (byte *)&sendpacket.seqNum;
	for (int i = 0; i < datasize + 5; i++) {
		sendBuffer[offset++] = *p++;
	}
}

void setup() {
	Serial.begin(9600);
	Bridge.begin();
	server.noListenOnLocalhost();
	server.begin();
	client = server.accept();
	srand((unsigned) time(NULL));
	randomSeed((unsigned) time(NULL));
	Angle = 0;
	servo.attach(7);
	servo.write(0);
}

int to_int(byte b) {
	return (b & 0xFFFF);
}

void initPacket() {
	memset(&recpacket, -1, TOTALL_PACK);
}

void loop() {
	if (client.connected()) {
		int i = 0;
		if (!clientActive) Serial.println("New client connection.");
		clientActive = true;

		if (client.available()) {
			while (client.available()) { packetBuffer[i] = client.read(); i++; }
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

		if (Serial.available()) {
			SerialChat = Serial.read();
			Serial.print(SerialChat);
			client.print(SerialChat);
		}
		
		switch (recpacket.code)
		{
		case Join:
			Serial.println("Start Send Packet");
			sendpacket.code = Response;
			sendpacket.id = 0;
			sendpacket.seqNum = random();
			FillSendBuffer(0);
			client.write(sendBuffer, TOTALL_PACK);
			Serial.println("Complete Send Packet");
			Serial.print("Send Seq_Num : ");
			Serial.println(sendpacket.seqNum);
			Angle = 45;
			initPacket();
			break;
		case UnLock:
			break;
		default:
			break;
		}
		
	}
	else {
		if (clientActive) {
			client.stop();
			Serial.println("Client disconnected.");
		}
		clientActive = false;
		client = server.accept();
	}
	servo.write(Angle);
	if (Angle > 0) {
		delay(100);
		Angle = 0;
	}
}