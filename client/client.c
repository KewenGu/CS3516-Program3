#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"

#define PACKET_SIZE sizeof(Packet)

#define END_OF_PHOTO_YES ((char)4)   // end of transmission
#define END_OF_PHOTO_NO ((char)3)    // end of text

#define END_OF_PACKET_YES ((char)4)  // end of transmission
#define END_OF_PACKET_NO ((char)3)   // end of text

#define DATALINK_EXPECTATION_ACK 1
#define DATALINK_EXPECTATION_

unsigned short seq_num = 0;

void printBuffer(char* buffer, int n) {
  for(int i = 0; i<n; i++)
     printf("%x", buffer[i]);

  printf("\n");
}

FILE* file2;

int main(int argc, char** argv) {

  file2 = fopen("test.jpg", "wb");

	if(argc != 4) {

		printf("Usage: ./client <servermachine> <id> <num_photos>\n");
		exit(1);

	}

	int clientID = atoi(argv[2]);
	int numPhotos = atoi(argv[3]);
  char fileName[129];
  FILE *file;
  int i, j, k;

	//Pointer to socket structure that ends up filled in by gethostbyname
	struct hostent *servHost;
	unsigned short port = WELLKNOWNPORT;
	servHost = gethostbyname(argv[1]);
	int sock = physical_Establish(servHost, port);

  if (send(sock, &clientID, sizeof(clientID), 0) < 0)
    DieWithError("send() failed");
  if (send(sock, &numPhotos, sizeof(numPhotos), 0) < 0)
    DieWithError("send() failed");

  //Packet memory allocation. First make space, then we can do the math for the packets easier
  for(i = 0; i < numPhotos; i++) 
  {
    sprintf(fileName, "photo%d%d.jpg", clientID, i);
    
    if((file = fopen(fileName, "rb")) == NULL) 
    {
      fprintf(stderr, "%s couldn't be found!\n", fileName);
      exit(1);
    }

    printf("To application layer\n");
    application_Layer(file, sock);
    printf("Send %s to the server\n", fileName);

    fclose(file);
  }
}


void DieWithError(char *errorMsg)
{
  perror(errorMsg);
  exit(1);
}


int physical_Establish(struct hostent* host, unsigned short port) {

  printf("Connecting to host on port %d\n", port);

	struct sockaddr_in serverAddress;
  int sock;

	  //Reset the struct
  	memset(&serverAddress, 0, sizeof(serverAddress));
  	serverAddress.sin_family = AF_INET;

  	//Copy the address from the gethostbyname struct into struct
  	memcpy(&serverAddress.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

  	//Convert the provided port to network byte order and assign to struct
  	serverAddress.sin_port = htons(port);

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    DieWithError("sock() failed");

	if(connect(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) 
    DieWithError("connect() failed");

  	return sock;

}


void application_Layer(FILE *file, int sock)
{
  fseek(file, 0, SEEK_END);
  int fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  int numPackets = (fileSize / PACKET_SIZE) + (fileSize % PACKET_SIZE > 0 ? 1 : 0);

  Packet *packets = malloc(numPackets * sizeof(Packet));

  int bytesLoaded = 0;
  int currentPacket = 0;
  int currentPosition = 0;
  int i;

  while(bytesLoaded < fileSize) {
    // Read from JPEG file one byte at a time
    fread(packets[currentPacket].data + currentPosition, 1, 1, file);

    currentPosition++;
    bytesLoaded++;
    // Start filling a new packet after PACKET_SIZE bytes read
    if(currentPosition == PACKET_SIZE) {
      //packets[currentPacket].endOfPhoto = END_OF_PHOTO_NO; // Indicate not end-of-photo
      currentPosition = 0;
      currentPacket++;
    }
  }

  //packets[numPackets - 1].endOfPhoto = END_OF_PHOTO_YES; // Indicate end-of-photo
  
  for (i = 0; i < numPackets; i++)
  {
    printf("To datalink layer\n");
    datalink_Layer(&packets[i], sizeof(packets[i]), sock);
  }



}

// Put the payload into the frame
void datalink_Layer(Packet *p, int packetSize, int sock)
{

  // First, initialize the frame
  int numFrames = (packetSize / FRAME_PAYLOAD_SIZE) + (packetSize % FRAME_PAYLOAD_SIZE > 0 ? 1 : 0);
  Frame *frames = (Frame *)malloc(numFrames * sizeof(Frame));

  printf("numFrames: %d\n", numFrames);
  
  int currentFrame = 0;
  int currentPosition = 0;
  int bytesToFrame = 0;
  int bytesFramed = 0;
  int totalBytesFramed = 0;
  int i;

  int frame = 0;
  int currentframepos = 0;

  for(int i = 0; i < packetSize; i++) {

    frames[frame].payload[currentframepos] = p->data[i];
    currentframepos++;
    if(currentframepos == FRAME_PAYLOAD_SIZE) {
      frames[frame].payloadLen = FRAME_PAYLOAD_SIZE;
      currentframepos = 0;
      frame++;
    }
  }
  /*
  for(int i = 0; i < numFrames; i++) {

    if(i < numFrames-1) {
      //Frame has full payload
      memcpy(frames[i].payload, p->data + currentPosition, FRAME_PAYLOAD_SIZE);
      frames[i].endOfPacket = END_OF_PACKET_NO;
      frames[i].frameType = FRAMETYPE_DATA;
      frames[i].seqNum[0] = seq_num & 0xff00;
      frames[i].seqNum[1] = seq_num & 0x00ff;

      currentPosition += FRAME_PAYLOAD_SIZE;
    }
    else if(i == (numFrames-1)) {
      memcpy(frames[i].payload, p->data + currentPosition, packetSize % FRAME_PAYLOAD_SIZE);
      frames[i].endOfPacket = END_OF_PACKET_YES;

      frames[i].frameType = FRAMETYPE_DATA;
      frames[i].seqNum[0] = seq_num & 0xff00;
      frames[i].seqNum[1] = seq_num & 0x00ff;

      currentPosition += (packetSize % FRAME_PAYLOAD_SIZE);
    }
    

    seq_num++;


  }
  */

  
  for(int i = 0; i < numFrames; i++) {
    printf("To physical layer with frame #: %x %x\n", frames[i].seqNum[0], frames[i].seqNum[1]);
    physical_Layer(&frames[i], sizeof(Frame), sock);
  }

  // Copy the packet into the frame payload
  /*while(totalBytesFramed < packetSize)
  {
    bytesFramed = 0;
    
    if (packetSize - totalBytesFramed > FRAME_PAYLOAD_SIZE)
      bytesToFrame = FRAME_PAYLOAD_SIZE;
    else
      bytesToFrame = packetSize - totalBytesFramed;

    for(i = 0; i < bytesToFrame; i++)
    {
      //printf("iteration %d of %d\n", i, FRAME_PAYLOAD_SIZE);
      //printf("currentPosition is %d and currentFrame is %d\n", currentPosition, currentFrame);

      frames[currentFrame].payload[i] = p->data[currentPosition];
      bytesFramed++;
      currentPosition++;
      // If reach the end-of-photo specifier
      if(frames[currentFrame].payload[i] == END_OF_PHOTO_YES || frames[currentFrame].payload[i] == END_OF_PHOTO_NO)
      {  
        frames[currentFrame].endOfPacket = END_OF_PACKET_YES;
        break;
      }
      
    }
    totalBytesFramed += bytesFramed;

    frames[currentFrame].frameType = FRAMETYPE_DATA;
    frames[currentFrame].seqNum[0] = seq_num & 0xff00;
    frames[currentFrame].seqNum[1] = seq_num & 0x00ff;

    
    frames[currentFrame].endOfPacket = END_OF_PACKET_NO;

    char *error_handling_result = error_Handling(frames[currentFrame], bytesFramed);

    frames[currentFrame].errorDetect[0] = error_handling_result[0];
    frames[currentFrame].errorDetect[1] = error_handling_result[1];

    printf("To physical layer with frame #: %d\n", seq_num);
    physical_Layer(&frames[currentFrame], FRAME_PAYLOAD_SIZE + 6, sock);

    seq_num++;
    currentFrame++;

  }

  */

  printf("Returning to application layer\n");

}


void physical_Layer(Frame* buffer, int frameSize, int sock) 
{
  int timeOut = 1;
  int notACKed = 1;
  //FrameACK *ack = malloc(sizeof(FrameACK));
  Frame *ack = malloc(sizeof(Frame));
  ack->frameType = FRAMETYPE_ACK;
  
  struct timeval timer;

  fd_set fileDescriptorSet;
  FD_ZERO(&fileDescriptorSet);
  FD_SET(sock, &fileDescriptorSet);

  printf("Physical send: length is %d\n", frameSize);

  while (timeOut)
    while (notACKed)
    {
      if (send(sock, buffer, frameSize, 0) < 0)
        DieWithError("send() error");
      
      timer.tv_sec = 3;
      timer.tv_usec = 0;

      printf("Start timer\n");
      //Frame timer
      if (select(sock + 1, &fileDescriptorSet, NULL, NULL, &timer) < 0)
        DieWithError("select() failed");

      if (timer.tv_sec == 0 && timer.tv_usec == 0) 
      {
        printf("Time out!\n");
        timeOut = 1; // time out!
        break;
      }
      else
      {
        printf("Receiving ACK\n");
        timeOut = 0; // not time out!
      
        //There's data to receive
        if (recv(sock, ack, sizeof(Frame), 0) < 0)
          DieWithError("recv() failed");

        printf("ACK received\n");

        printf("seq_num = %x %x\n", buffer->seqNum[0], buffer->seqNum[1]);
        printf("ack->seqNum = %x %x\n", ack->seqNum[0], ack->seqNum[1]);
        printf("ack->errorDetect = %d\n", ack->errorDetect);

        if(1)
        //if (atoi(ack->seqNum) == seq_num && atoi(ack->errorDetect) == atoi(ack->seqNum))
        {
          notACKed = 0; // ACK successful!
          break;
        }
        else
        {
          printf("ACK failed!\n");
          notACKed = 1; // ACK failed!
        }
      }
    }

    printf("Sending frame successfully!\n");

}

/* Function generates the error detection bytes
    how this work?
        suppose the frame in hex representation is "00 01 02 03 04 05 06 07... [2 error detection bytes]"
        then, error detection bytes = 00^02^04^06... + 01^03^05^07...  (^ is the operation of XOR, + is the operation of concatenation)
*/
char *error_Handling(Frame t, int size)
{
  int i;
  char *result = malloc(2 * sizeof(unsigned char));

  for (i = 0; i < (size - 2); i += 2) {

    result[0] = *(unsigned char *)&t ^ result[0];
  }

  for (i = 1; i < (size - 2); i += 2) {

    result[1] = *(unsigned char *)&t ^ result[1];
  }

  return result;
}


