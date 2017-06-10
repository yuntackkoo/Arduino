#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <time.h>
#include <Servo.h>

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
	*pointer++ = packet[offset++]; //code�� ����
	*pointer++ = packet[offset++]; //�е������ ����
	offset += recpacket.paddingSize; //�е��κ��� ����ħ
	pointer += recpacket.paddingSize; //�е��κ��� ����ħ
	for (int i = offset; i<TOTALL_PACK; i++) {
		*pointer++ = packet[offset];//�е��κ� ���� ������ �����͸� ����
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