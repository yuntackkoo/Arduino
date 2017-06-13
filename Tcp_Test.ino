#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <time.h>
#include <Servo.h>

#define PORT 255
#define TOTALL_PACK 33    //�� ��Ŷ ������ 33Byte
#define MAX_CLIENT 5
#define MAX_USER 12

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
YunClient client[MAX_CLIENT];
long seq_num_arr[MAX_CLIENT];
String usertable[MAX_USER];

static boolean clientActive = false;
char SerialChat;

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
Servo servo;
int Angle;
unsigned int cuid = 0;

void Parsing(byte *packet) {
	int offset = 0;
	byte *pointer = (byte *)&recpacket.code;
	*pointer++ = packet[offset++]; //code�� ����
	*pointer++ = packet[offset++]; //�е������ ����
	offset += recpacket.paddingSize; //�е��κ��� ����ħ
	pointer += recpacket.paddingSize; //�е��κ��� ����ħ
	recpacket.datasize = TOTALL_PACK - 7 - recpacket.paddingSize;
	for (int i = offset; i<offset+4; i++) {
		*pointer++ = packet[i];//�е��κ� ���� ������ �����͸� ����
	}
	offset += 5;
	for (int i = 0; i < recpacket.datasize; i++) {
		recpacket.data[i] = packet[offset++];
	}
}

void FillSendBuffer(int datasize) {
	int offset = 0;
	sendBuffer[offset++] = sendpacket.code;
	sendpacket.paddingSize = TOTALL_PACK - datasize - 7;
	sendpacket.datasize = 26 - sendpacket.paddingSize;
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

void SwitchPacket(int number);

void setup() {
	Serial.begin(9600);
	Bridge.begin();
	server.noListenOnLocalhost();
	server.begin();
	srand((unsigned) time(NULL));
	randomSeed((unsigned) time(NULL));
}

int to_int(byte b) {
	return (b & 0xFFFF);
}

void initPacket() {
	recpacket.code = -1;
}

void loop() {
	int i = 0;
	for (i = 0; i < MAX_CLIENT; i++) {
		if (!client[i].connected()) {
			client[i] = server.accept();
			
		}
	}

	for (i = 0; i < MAX_CLIENT; i++) {
		if (client[i].connected()) {
			SwitchPacket(i);
		}
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

		if (Serial.available()) {
			SerialChat = Serial.read();
			Serial.print(SerialChat);
			client[number].print(SerialChat);
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
						client[i].write(sendBuffer,TOTALL_PACK);
					}
				}
			}
			digitalWrite(13, HIGH);
			delay(1000);
			digitalWrite(13, LOW);
			break;
		default:
			break;
		}
		initPacket();
	}
}