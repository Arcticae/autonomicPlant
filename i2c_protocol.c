#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <unistd.h>
#define	SDA	9 //blue led for data - other one for clock
#define SCL 10
//for testing purposes
#define SIG_TIME 3000000
#define SLEEP_TIME 500000
//send high - when clock is high the data transfer is relevant -> clock streching is holding the line low to prevent data flow.
char reset_sda[19];
char reset_scl[19];

int reset_gpio(){
	if(system(reset_sda)!= 0 || system(reset_scl)!=0){
		perror("Failure in resetting gpios");
		return 1;
	}
	return 0;
}
//send functions -> both set the sda line to corresponding state and give a clock sig
void send_high(){
	digitalWrite(SDA,HIGH);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SCL,HIGH);
	delayMicroseconds(SIG_TIME);
	digitalWrite(SCL,LOW);
	delayMicroseconds(SLEEP_TIME);
	
}
void send_low(){
	digitalWrite(SDA,LOW);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SCL,HIGH);
	delayMicroseconds(SIG_TIME);
	digitalWrite(SCL,LOW);
	delayMicroseconds(SLEEP_TIME);
}
int receive_bit(){
	//set sda to high (default value)
	digitalWrite(SDA,HIGH);
	delayMicroseconds(SLEEP_TIME);
	pinMode(SDA,INPUT);
	delayMicroseconds(SLEEP_TIME);
	//Generate clock pulse
	digitalWrite(SCL,HIGH);
	delayMicroseconds(SLEEP_TIME);
	//while clock is up data shoudl be on sda
	int read = digitalRead(SDA);
	//pull down clock
	digitalWrite(SCL,LOW);
	delayMicroseconds(SLEEP_TIME);
	//and return to wrtiing on sda
	pinMode(SDA,OUTPUT);
	return read;
}
//start and end transaction on a i2c bus
// start -> pull first scl low, then pull sda low.
void start_tx(){
	setup_i2c();
	digitalWrite(SDA,LOW);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SCL,LOW);
	delayMicroseconds(SLEEP_TIME);
	
}
//stop -> pull scl and sda low, then first pull up scl and sda after.
void end_tx(){
	digitalWrite(SCL,LOW);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SDA,LOW);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SCL,HIGH);
	delayMicroseconds(SLEEP_TIME);
	digitalWrite(SDA,HIGH);
	delayMicroseconds(SLEEP_TIME);
	reset_gpio();
}

void send_byte(int byte){
	int bits[8];
	int i;
	for(i=7;i>=0;i--){
		bits[i]=byte%2;
		byte/=2;
	}
	for(i = 0 ;i<8;i++){
		if(bits[i] ==0)
			send_low();
		else
			send_high();
	}
}
int receive_byte(){
	int bits[8];
	int i;
	for(i=7;i>=0;i--){
		bits[i]=receive_bit();
		delayMicroseconds(SLEEP_TIME);
	}
	int ret = 0;
	for(i = 0 ; i < 8 ;i++){
		ret*=2;
		ret+=bits[i];
	}
	delayMicroseconds(SLEEP_TIME);
	return ret;
}
//args -> slave addr is address of a slave, we then shift one bit to the left
// then we add 1 to the addr if we want to read from it.  
int read_from_slave(int slave_addr,int reg_addr){
		int slave_adrr = (slave_addr<<1) + 1;
		//start sequence
		start_tx();
		//send the address and wait for response
		send_byte( slave_addr );
		if(receive_bit()!=0){
			printf("Error in transmitting data - no such device or could not handle the message\n");
		}
		//send the wanted address of the data
		send_byte( reg_addr );
		if(receive_bit()!=0){
			printf("Error in transmitting data - no such device or could not handle the message\n");
		}
		//receive wanted data.
		int readval = receive_byte();
		
		//ending sequence
		end_tx();
}
//We have to remember when setting up that the i2c protocol specifies that the link is pulled up.
void setup_i2c(void){
	//wiringPiSetupGpio();
	pinMode(SCL,OUTPUT);
	digitalWrite(SCL,HIGH);
	delayMicroseconds(SLEEP_TIME);
	pinMode(SDA,OUTPUT);
	digitalWrite(SDA,HIGH);
	delayMicroseconds(SLEEP_TIME);
	sprintf(reset_sda,"gpio -g mode %d alt0",SDA);
	sprintf(reset_scl,"gpio -g mode %d alt0",SCL);
	
}


int main(void){
	wiringPiSetupGpio();
	setup_i2c();
	
	int i;
	printf("Setup done. Execution begins...\n");
	
	printf("Delay...\n");
	usleep(500000);
	
	//for(int i =0; i<10; i++){
	send_high();
	send_low();
	//}
	send_byte(2);
	if(reset_gpio()!=0){
		printf("Failed to reset some connections, reset the pin 2 and 3 maunally to alt0\n");
		return 1;
	}else{
		printf("Successful gpio reset, exiting...\n");
	}
	return 0;
}
