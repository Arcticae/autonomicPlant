#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define    SDA    9 //blue led for data - other one for clock
#define SCL 10
//for testing purposes
#define SIG_TIME 50
#define SLEEP_TIME 50
//send high - when clock is high the data transfer is relevant -> clock streching is holding the line low to prevent data flow.

#define HDC1080_ADDR 0x40
#define HDC1080_REG_TEMP 0x00
#define HDC1080_REG_HUM 0x01
#define HDC1080_REG_CONFIG 0x02

#define CCS811_ADDR 0x5A
#define CCS811_ENV_DATA_REG 0x05
#define CCS811_REG_ALG_RESULT 0x02
#define CCS811_APP_START 0xF4
#define CCS811_MEAS_MODE 0x01

char reset_sda[19];
char reset_scl[19];

int reset_gpio() {
    if (system(reset_sda) != 0 || system(reset_scl) != 0) {
        perror("Failure in resetting gpios");
        return 1;
    }
    return 0;
}

//send functions -> both set the sda line to corresponding state and give a clock sig
void send_high() {
    digitalWrite(SDA, HIGH);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(SIG_TIME);
    digitalWrite(SCL, LOW);
    delayMicroseconds(SLEEP_TIME);

}

void send_low() {
    digitalWrite(SDA, LOW);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(SIG_TIME);
    digitalWrite(SCL, LOW);
    delayMicroseconds(SLEEP_TIME);
}

int receive_bit() {
    //set sda to high (default value)
    digitalWrite(SDA, HIGH);
    delayMicroseconds(SLEEP_TIME);
    pinMode(SDA, INPUT);
    delayMicroseconds(SLEEP_TIME);
    //Generate clock pulse
    digitalWrite(SCL, HIGH);
    delayMicroseconds(SLEEP_TIME);
    //while clock is up data shoudl be on sda
    int read = digitalRead(SDA);
    //pull down clock
    digitalWrite(SCL, LOW);
    delayMicroseconds(SLEEP_TIME);
    //and return to wrtiing on sda
    pinMode(SDA, OUTPUT);
    return read;
}

//start and end transaction on a i2c bus
// start -> pull first scl low, then pull sda low.
void start_tx() {
    setup_i2c();
    digitalWrite(SDA, LOW);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SCL, LOW);
    delayMicroseconds(SLEEP_TIME);

}

//stop -> pull scl and sda low, then first pull up scl and sda after.
void end_tx() {
    digitalWrite(SCL, LOW);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SDA, LOW);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(SLEEP_TIME);
    digitalWrite(SDA, HIGH);
    delayMicroseconds(SLEEP_TIME);
    reset_gpio();
}

void send_byte(int byte) {
    int bits[8];
    int i;
    for (i = 7; i >= 0; i--) {
        bits[i] = byte % 2;
        byte /= 2;
    }
    for (i = 0; i < 8; i++) {
        if (bits[i] == 0)
            send_low();
        else
            send_high();
    }
}

int receive_byte() {
    int bits[8];
    int i;
    for (i = 0; i < 8; i++) {
        bits[i] = receive_bit();
        printf("%d",bits[i]);
        delayMicroseconds(SLEEP_TIME);
    }
    printf("\n");
    int ret = 0;
    for (i = 0; i < 8; i++) {
        ret *= 2;
        printf("%d",bits[i]);
        ret += bits[i];
    }
    printf("\n");
    delayMicroseconds(SLEEP_TIME);
    return ret;
}

//args -> slave addr is address of a slave, we then shift one bit to the left
// then we add 1 to the addr if we want to read from it.  
void write_to_slave(int slave_addr, int reg_addr, int *data) {
    slave_addr = (slave_addr << 1);
    //start sequence
    start_tx();
    //send the address and wait for response
    send_byte(slave_addr);
    if (receive_bit() != 0) {
        printf("Error in transmitting data - no such device or could not handle the message(slave-addres)\n");
    }
    //send the wanted address of the data
    send_byte(reg_addr);
    if (receive_bit() != 0) {
        printf("Error in transmitting data - no such device or could not handle the message(register-addres)\n");
    }
    //receive wanted data
    if(data!=0){
    for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
        send_byte(data[i]);
        if (receive_bit() != 0) {
            printf("Error in transmitting data - no such device or could not handle the message(data)\n");
        }
    }
	}
    //ending sequence
    end_tx();
}

//args -> slave addr is address of a slave, we then shift one bit to the left
// then we add 1 to the addr if we want to read from it.
int read_from_slave(int slave_addr, int reg_addr) {
    slave_addr = (slave_addr << 1);
    //start sequence
    start_tx();
    //send the address and wait for response
    send_byte(slave_addr);
    if (receive_bit() != 0) {
        printf("Error in transmitting data - no such device or could not handle the message(slave-addres)\n");
    }
    //send the wanted address of the data
    send_byte(reg_addr);
    if (receive_bit() != 0) {
        printf("Error in transmitting data - no such device or could not handle the message(register-addres)\n");
    }
    end_tx();
    start_tx();
    
    
    slave_addr = slave_addr + 1;
    send_byte(slave_addr);
    
    if (receive_bit() != 0) {
        printf("Error in transmitting data - no such device or could not handle the message(slave-addres-2)\n");
    }
    //receive wanted data.
    
    int readval = receive_byte();
	send_low();
	
	printf("%d - first byte\n", readval);
	readval = (readval << 8);
	readval = readval + receive_byte();
	send_high();
    //ending sequence
    end_tx();
    
    return readval;
}

//We have to remember when setting up that the i2c protocol specifies that the link is pulled up.
void setup_i2c(void) {
    //wiringPiSetupGpio();
    pinMode(SCL, OUTPUT);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(SLEEP_TIME);
    pinMode(SDA, OUTPUT);
    digitalWrite(SDA, HIGH);
    delayMicroseconds(SLEEP_TIME);
    sprintf(reset_sda, "gpio -g mode %d alt0", SDA);
    sprintf(reset_scl, "gpio -g mode %d alt0", SCL);

}

void print_temp_hum_CO2(double temp, double humidity, int CO2){
    printf("Temperature: %.2f C\n", temp);
    printf("Humidity: %.2f %\n", humidity);
    printf("CO2: %dppm\n", CO2);
    printf("\n");
}

double read_temperature(){
    write_to_slave(HDC1080_ADDR, HDC1080_REG_TEMP, 0);
    delayMicroseconds(65);
    int read_temp = read_from_slave(HDC1080_ADDR, HDC1080_REG_TEMP);
    delayMicroseconds(15);
    
    double temp = ((double)read_temp/(1<<16))*165.0-40.0;
    return temp;
}

double read_humidity(){
    write_to_slave(HDC1080_ADDR, HDC1080_REG_HUM, 0);
    delayMicroseconds(65);
    int read_hum = read_from_slave(HDC1080_ADDR, HDC1080_REG_HUM);
    delayMicroseconds(15);
    
    double humidity = ((double)read_hum/(1<<16)) * 100;
    return humidity;
}

void set_compensation(double temperature, double humidity){
    delayMicroseconds(15);
    int hum1 = (int) (humidity/0.5);
    int hum2 = (int) (humidity * 512 - hum1 * 256);
    int tmp1 = (int) (temperature / 0.5);
    int tmp2 = (int) (temperature * 512 - tmp1 * 256);
    int data[] = {5, hum1, hum2, temp1, temp2, 0x00};
    write_to_slave(CCS811_ADDR, CCS811_ENV_DATA_REG, data);
}

int read_CO2(){
    delayMicroseconds(250);
    delayMicroseconds(15);
    write_to_slave(CCS811_ADDR, CCS811_REG_ALG_RESULT, 0);
    delayMicroseconds(65);
    int read_CO2_result = read_from_slave(CCS811_ADDR, CCS811_REG_ALG_RESULT);
    return read_CO2_result;
}

void init_CCS811(){
    // configure sensor
    delayMicroseconds(15);
    write_to_slave(CCS811_ADDR, CCS811_APP_START, 0);
    delayMicroseconds(65);
    
    int data[] = {1<<5, 0};
    write_to_slave(CCS811_ADDR, CCS811_MEAS_MODE, data);
    delayMicroseconds(15);
}

int main(void) {
    wiringPiSetupGpio();
    setup_i2c();
    delayMicroseconds(15);
    /********************/
    //
    // 00010000 00000000
    // 12th bit to 1 (to set measurement both temp and humidity)
    // 10th bit to 0 (14 bit temp resolution)
    // 9th, 8th bit to 0 (14 bit humidity resolution)
    int data[] = {1<<4, 0<<8};
    write_to_slave(HDC1080_ADDR, HDC1080_REG_CONFIG, data);
    delayMicroseconds(15);  //
    /*******************/
    
    init_CCS811();

    

    int i;
    printf("Setup done. Execution begins...\n");

    printf("Delay...\n");
    delayMicroseconds(15);
    
    for(int i = 0; i < 10; i++){
        double temperature = read_temperature();
        double humidity = read_humidity();
        set_compensation(temperature, humidity);
        int CO2 = read_CO2;
        
        print_temp_hum_CO2(temperature, humidity, CO2);
        delayMicroseconds(1000);
    }

    


    
    if (reset_gpio() != 0) {
        printf("Failed to reset some connections, reset the pin 2 and 3 maunally to alt0\n");
        return 1;
    } else {
        printf("Successful gpio reset, exiting...\n");
    }
    return 0;
}
