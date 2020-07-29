#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include <snap7.h>
#include <hiredis.h>


#ifdef OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

    S7Object Client;
    unsigned char Buffer[65536]; // 64 K buffer

    char *Address;          // PLC IP Address
    int Rack = 0, Slot = 0; // Default Rack and Slot on PLC

    int JobDone = false;
    int JobResult = 0;

//------------------------------------------------------------------------------
//  Async completion callback
//------------------------------------------------------------------------------
void S7API CliCompletion(void *usrPtr, int opCode, int opResult){
    JobResult=opResult;
    JobDone = true;
}
//------------------------------------------------------------------------------
// Unit Connection
//------------------------------------------------------------------------------
int CliConnect(){
    int res;

    res = Cli_ConnectTo(Client, Address,Rack,Slot);
    return !res;
}
//------------------------------------------------------------------------------
// Unit Disconnection
//------------------------------------------------------------------------------
void CliDisconnect(){
     Cli_Disconnect(Client);
}
//------------------------------------------------------------------------------
// My print in HEX function
//------------------------------------------------------------------------------
void hexdump(void* pt, int len){
	printf("\n");
	if(len % 16 != 0)
		len = (len/16 + 1);
	else
		len = len/16;
	for(int i = 0; i < len; i++){
		printf("\n%8.8x", (unsigned char*)pt + i);
		printf("  ");
		for(int j = 0; j < 16; j++){
			printf(" %2.2x", *((unsigned char*)pt + j + i*16));
		}
		printf("    ");
		for(int j = 0; j < 16; j++){
			if(isprint(*((char*)pt + j + i*16)))
				printf("%c",*((char*)pt + j + i*16));
			else
				printf(".");
		}
	}
}
//------------------------------------------------------------------------------
// Swap numbers for SPS (Type: "Int" / "Short Int")
//------------------------------------------------------------------------------
void swap_int(void* pointer){
	unsigned char *pointer1 = (unsigned char *)pointer;
	unsigned char help = *pointer1;
	*pointer1 = *(pointer1+1);
	*(pointer1+1) = help;
}
//------------------------------------------------------------------------------
// Swap numbers for SPS (Type: "Real" / "Float")
//------------------------------------------------------------------------------
void swap_real(void* pointer){
	unsigned char *pointer1 = (unsigned char*)pointer;
	unsigned char help = *pointer1;
	*pointer1 = *(pointer1+3);
	*(pointer1+3) = help;
	unsigned char *pointer2 = (unsigned char*)pointer;
	help = *(pointer2+1);
	*(pointer2+1) = *(pointer2+2);
	*(pointer2+2) = help;
}
//------------------------------------------------------------------------------
// Read real from SPS
//------------------------------------------------------------------------------
float read_real(int byteOffset1, int dbnr1){
	byte db[4] = {0};
	float newNum;
	//Read from DB
	Cli_DBRead(Client, dbnr1, byteOffset1, 4, &db[0]);
	//Swap bytes --> copy bytes to 'newNum' --> return 'newNum'
	swap_real(db);
	memcpy(&newNum, &db, sizeof(newNum));
	return newNum;
}
//------------------------------------------------------------------------------
// Write integer into sps
//------------------------------------------------------------------------------
void write_int(int byteOffset1, int dbnr1, short int writeNumber){
	swap_int(&writeNumber);
	Cli_DBWrite(Client, dbnr1, byteOffset1, 2, &writeNumber);
}



int main()
{
	//Wait for Redis-Server to start - 5 seconds just to be safe
	sleep(5);
	short int customRev = 0;
	float torque = 0, current = 0, power = 0, rev = 0;
	//Connect to REDIS and check if error occured and print it
	redisContext *c = redisConnect("10.2.150.143", 6379);
	if (c != NULL && c->err)
    	printf("Error: %s\n", c->errstr);
	else
    	printf("Connected to Redis\n");
	redisReply *reply;
	//Connect to PLC
  Address="10.2.150.40";
	Client=Cli_Create();
  Cli_SetAsCallback(Client, CliCompletion, NULL);
	while(1){
		if (CliConnect()){
				//Read data from REDIS
				reply = redisCommand(c, "GET %s", "CustomRev");
				customRev = (short)atoi(reply->str);
				freeReplyObject(reply);

				//Read data from PLC
				torque = read_real(0, 1);
				current = read_real(4, 1);
				power = read_real(8, 1);
				rev = read_real(12, 1);

				//Write data to PLC
				write_int(18, 1, customRev);
				CliDisconnect();

				//Write data to REDIS
				reply = redisCommand(c, "SET %s %.2f", "Torque", torque);
				freeReplyObject(reply);
				reply = redisCommand(c, "SET %s %.2f", "Current", 	current);
				freeReplyObject(reply);
				reply = redisCommand(c, "SET %s %.2f", "Power", power);
				freeReplyObject(reply);
				reply = redisCommand(c, "SET %s %.2f", "Rev", rev);
				freeReplyObject(reply);
		}
	}

	redisFree(c);
  Cli_Destroy(&Client);
  return 0;
}
