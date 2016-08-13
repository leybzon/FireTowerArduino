/***************************************************************************

Copyright 2016 Gene Leybzon

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

***************************************************************************/


/*
 *  Arduino code for the Burning Man project "Fire Tower"
 *  Control the Fire at the Atom Cult Camp
 */

#include <SoftwareSerial.h>

#define FIRST_FIRE_PIN 24  //First pin connected to the gas switch
#define FIRE_CHANNELS  22  //Number of channels that will deliver gas to burners

#define RESPONSE_BUFFER_LENGTH 100  //Buffer length for data from the mobile device (phone)


SoftwareSerial ble(2,3);				// For Uno, HM10 TX pin to Arduino Uno pin D2, HM10 RX pin to Arduino Uno pin D3
//SoftwareSerial ble(10,11);			        // For Mega 2560, HM10 TX pin to Arduino Mega 2650 pin D10, HM10 RX pin to Arduino Mega 2560 pin D11

char buffer[RESPONSE_BUFFER_LENGTH];				// Buffer to store response
int timeout=1000; 						// Wait 1000ms each time for BLE to response, depending on your application, adjust this value accordingly
long bauds[] = {
  9600,57600,115200,38400,2400,4800,19200};	// common baud rates, when using HM-10 module with SoftwareSerial, try not to go over 57600

//Find the baund rate for BTLE modem
long BLEAutoBaud() {
  int baudcount=sizeof(bauds)/sizeof(long);
  for(int i=0; i<baudcount; i++) {
    for(int x=0; x<3; x++) { 		// test at least 3 times for each baud
      Serial.print("Testing baud ");
      Serial.println(bauds[i]);
      ble.begin(bauds[i]);
      if (BLEIsReady()) {
        return bauds[i];
      }
    }
  }
  return -1;
}


//Check the BTLE modem
boolean BLEIsReady() {
  BLECmd(timeout, "AT" ,buffer);		// Send AT and store response to buffer 
  if (strcmp(buffer,"OK")==0){		
    return true;
  } 
  else {
    return false;
  }  
}

//Send command to BTLE
boolean BLECmd(long timeout, char* command, char* temp) {
  long endtime;
  boolean found=false;
  endtime=millis()+timeout;			// 
  memset(temp,0,100);					// clear buffer
  found=true;
  Serial.print("Arduino send = ");
  Serial.println(command);
  ble.print(command);

  // The loop below wait till either a response is received or timeout
  // The problem with this BLE Shield is the HM-10 module does not response with CR LF at the end of the response,
  // so a timeout is required to detect end of response and also prevent the loop locking up.

  while(!ble.available()){
    if(millis()>endtime) {			// timeout, break
      found=false;
      break;
    }
  }  

  if (found) {						// response is available
    int i=0;
    while(ble.available()) {		// loop and read the data
      char a=ble.read();
      // Serial.print((char)a);	// Uncomment this to see raw data from BLE
      temp[i]=a;					// save data to buffer
      i++;
      if (i>=RESPONSE_BUFFER_LENGTH) break;	// prevent buffer overflow, need to break
      delay(1);						// give it a 2ms delay before reading next character
    }
    Serial.print("BLE reply    = ");
    Serial.println(temp);
    return true;
  } 
  else {
    Serial.println("BLE timeout!");
    return false;
  }
}

void setup() {
  Serial.begin(9600); 

  pinMode(13, OUTPUT);
  digitalWrite(13,LOW);


  for(int i=FIRST_FIRE_PIN; i<FIRE_CHANNELS; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i,LOW);
  }

  // If you see lots of BLE timeout on Serial Monitor after BLEAutoBaud completed, most likely your have a bad shield
  // Check if the shield JUMPER is correctly set to 
  // HM10 TX to D2
  // HM10 RX to D3
  long baudrate = BLEAutoBaud();

  if (baudrate>0) {
    Serial.print("Found BLE baud rate ");
    Serial.println(baudrate);
  } 
  else {
    Serial.println("No BLE detected.");
    while(1){
    };						// No BLE found, just going to stop here
  }


  //Prepare BTLE
  BLECmd(timeout,"AT+NAME?",buffer);
  BLECmd(timeout,"AT+BAUD?",buffer);
  BLECmd(timeout,"AT+MODE?",buffer);
  BLECmd(timeout,"AT+PASS?",buffer);
  BLECmd(timeout,"AT+VERS?",buffer);
  BLECmd(timeout,"AT+RADD?",buffer);
  BLECmd(timeout,"AT+ADDR?",buffer);
  BLECmd(timeout,"AT+TYPE?",buffer);
  BLECmd(timeout,"AT+POWE?",buffer);
  BLECmd(3*timeout,"AT+NAMEBurn",buffer);
  BLECmd(timeout,"AT+ROLE0",buffer);
  BLECmd(timeout,"AT+ROLE?",buffer);
  BLECmd(timeout,"AT+HELP?",buffer);

  Serial.println("----------------------");
  Serial.println("Waiting for remote connection...");
}

//switch fire off for all channels
void fireOffAll() {
  //Serial.println("Fire off");
  for(int i=FIRST_FIRE_PIN; i<FIRE_CHANNELS; i++) {
    digitalWrite(i,LOW);
  }
}

//switch fire on for specified channel
void fireOn(int channel) {
  Serial.print("Fire: ");
  if (channel>=0 && channel<FIRE_CHANNELS) {
    Serial.println(channel);
    digitalWrite(channel, HIGH);
  }
}

//command to tower
//Format:  <time_to_burn>, fire_channel [, fire_channel] ...
void towerCommand(char* cmd) {
  char * pch;
  pch = strtok (cmd,",");
  int cnt = 0;
  int fireTime = 100;  //[ms]

  fireOffAll();

  while (pch != NULL) {
    //printf ("%s\n",pch);
    if (cnt==0) {
      fireTime = atoi(pch);
    } 
    else {
      fireOn(atoi(pch));
    }

    pch = strtok(NULL, ",");
    cnt++;
  }

  //delay(fireTime);
  fireOffAll();

}

char inData[512];  
byte index = 0;

//Main event loop
void loop() {

  while(ble.available() > 0) {
    char aChar = (char)ble.read();
    if(aChar == '\n') {
      // End of record detected. Time to parse
      Serial.println(inData);  //cmd
      towerCommand(inData);

      index = 0;
      inData[index] = NULL;
    } 
    else {
      if (aChar == ',' || isDigit(aChar)) {
        inData[index] = aChar;
        index++;
        inData[index] = '\0'; // Keep the string NULL terminated
      }
    }
  }


  delay(10);    
}

