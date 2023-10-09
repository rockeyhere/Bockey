
// Author:Rockey

#include <stdio.h>
#include <string.h>

//include the SD library:
#include <SPI.h>
#include <SD.h>

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

// Maduino zero 4G LTE: pin 4 for SD Card
const int PIN_SD_SELECT = 4;

File myFile;

#define DEBUG true
#define MODE_1A

#define DTR_PIN 9
#define RI_PIN 8

#define LTE_PWRKEY_PIN 5
#define LTE_RESET_PIN 6
#define LTE_FLIGHT_PIN 7

String from_usb = "";
String incomingString;
long callStartTime = -1;
long optTimer = 0;
int isCalling = 0;
int isTexting = 0;
int callTimeLimit; 
const String rockeyNum = "+16173960660";
bool myModuleState = false;

//test-related variables
uint16_t battery_voltage;
int battInit;
int battNow;
long battInterval;
long lastBattCheckTime;

//// high level logic
/*
 * Wake Up (every [internval]) (10 minutes? end of an hour)
 *
 * check if there is cell signal
 *
 * check battery enough to perform
 * perform
 *
 * 1/ yes
 *
 *  check sms to see if there is phone call request
 *  - make phone call
 *  - rec
 *
 * 2/ iffy
 *
 * send text saying it is iffy
 *
 * 3/ no
 * go back to sleep
 *
 * end perform
 * go to sleep
 *
 */

void setup()
{
    SerialUSB.begin(115200);
    delay(100);
    Serial1.begin(115200);
    delay(100);

    //Serial1.begin(UART_BAUD, SERIAL_8N1, MODEM_RXD, MODEM_TXD);

    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);

    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    delay(100);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(7000);
    //digitalWrite(LTE_PWRKEY_PIN, LOW);

    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW); //Normal Mode
    // digitalWrite(LTE_FLIGHT_PIN, HIGH);//Flight Mode

    while(!turnOnModule){ 

      if (!Serial1){
        Serial1.begin(115200);
        delay(100);
        } //wait till Serial1 turn on

    };
      
    myModuleState = true;
    moduleConfigure();

    //// setting parameters

    //battery
    lastBattCheckTime = 0;
    battInterval = 300000;

    //calltime in ms
    callTimeLimit = 120000;
}

void loop()
{   

    if (millis()>(lastBattCheckTime+battInterval)){
      lastBattCheckTime = millis();
      reportBatt();
      moduleConfigure();
    }
    
    //make sure serial1 opens, and reset manually
    while (!Serial1){   
        Serial1.begin(115200);
        delay(2000);
        sendData("AT+CNMI = 2,2,0,0,0", 3000, DEBUG);
        sendData("AT+CMGF = 1", 3000, DEBUG);
        sendData("AT+CSMP = 17,167,0,0", 3000, DEBUG);
    }

    //while (Serial1.available() > 0 || incomingString != "")
    while (Serial1.available() > 0)
    {
        incomingString = Serial1.readStringUntil('\n\r');
        SerialUSB.println(incomingString);

        //answer call if rinng
        if (incomingString.indexOf("RING") >0){
           sendData("ATA", 3000, DEBUG);
           callStartTime = millis();
           isCalling = 1;
        }
        
        incomingString.toLowerCase();

        ////text back settings: scanning the text input to respond respective content
        if (incomingString.indexOf("lonely") >= 0){
           sendText("Hey...It's okay, I'm here.",rockeyNum,3000,DEBUG);
        }
        if (incomingString.indexOf("battery") >= 0){
          reportBatt();
        }
        if (incomingString.indexOf("thanks") >=  0){
           sendText("You are Welcome.",rockeyNum,3000,DEBUG);
        }
       
        //SerialUSB.write(Serial1.read());
        incomingString = "";
        yield();
    }
   
   //call timer
    if (isCalling == 1)
    {
        int callTime = millis()-callStartTime;
        //SerialUSB.println(callTime); //for testinng
        if (callTime >= callTimeLimit)
        {
        sendData("AT+CHUP", 3000, DEBUG); //hang up the phone
        isCalling = 0;
        }
    }

//    //could be move to global
//    int gpsTimer = millis();
//    String gpsNow = "";
//
//    if (millis()-gpsTimer>=15000){
//      gpsNow = sendData("AT+CGPSINFO", 3000, DEBUG);
//      if (!gpsNow.equals(""))
//        {
//            SerialUSB.println(gpsNow);
//        }
//      gpsTimer = millis();
//    }
   
   
    while (SerialUSB.available() > 0)
    {
#ifdef MODE_1A
        int c = -1;
        c = SerialUSB.read();
        if (c != '\n' && c != '\r')
        {
            from_usb += (char)c;
        }
        else
        {
            if (!from_usb.equals(""))
            {
                sendData(from_usb, 0, DEBUG);
                from_usb = "";
                yield();
            }
        }
#else
        Serial1.write(SerialUSB.read());
        yield();
#endif
    }
}



//sending Command to the Serial1
String sendData(String command, const int timeout, boolean debug)
{
    String response = "";
    if (command.equals("1A") || command.equals("1a"))
    {
        SerialUSB.println();
        SerialUSB.println("Get a 1A, input a 0x1A");

        //Serial1.write(0x1A);
        Serial1.write(26);
        Serial1.println();
        return "";
    }
    else
    {
        Serial1.println(command);
    }

    long int time = millis();
    while ((time + timeout) > millis())
    {
        while (Serial1.available())
        {
            char c = Serial1.read();
            response += c;
        }
    }
    if (debug)
    {
        SerialUSB.print(response);
    }
    return response;
}

//sending sms message
String sendText (String content, String cellNumber, const int timeout, boolean debug){
 
  //check if content is valid (length range)
  //check if number is valid?

  //right now number is not used, a constant is
  const String rockeyNum = "+16173960660";
  String command = "AT+CMGS = \""+String(cellNumber)+"\";";

  //sending
  sendData(command, timeout, DEBUG);
  //sendData(content, timeout, DEBUG);
  Serial1.print(content); //test to get rid of the extra line
  sendData("1A", timeout, DEBUG);
  }

void SDtest(){
  myFile = SD.open("test.txt", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    SerialUSB.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");
    // close the file:
    myFile.close();
    SerialUSB.println("done.");
  } else {
    // if the file didn't open, print an error:
    SerialUSB.println("error opening test.txt");
  }

  // re-open the file for reading:
  myFile = SD.open("test.txt");
  if (myFile) {
    SerialUSB.println("test.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      SerialUSB.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    SerialUSB.println("error opening test.txt");
  }
}

bool turnOnModule()
    {
        if (!moduleStateCheck()) // if it's off, turn it on.
        {
#ifdef atmelsam
            digitalWrite(LTE_PWRKEY_PIN, LOW);
            delay(500);
            digitalWrite(LTE_PWRKEY_PIN, HIGH);
#endif
#ifdef espressif32
            digitalWrite(LTE_PWRKEY_PIN, 1);
            delay(500);
            digitalWrite(LTE_PWRKEY_PIN, 0);
#endif
            for (int i = 0; i < 40; i++)
            {
                if (moduleStateCheck())
                {
                    return true;
                }
                delay(250);
            }

            return false;
        }

        return true;
    }

bool moduleStateCheck()
{
    int i = 0;
    bool moduleState = false;
    for (i = 0; i < 5; i++)
    {
        String msg = String("");
        msg = sendData("AT", 1000, DEBUG);
        if (msg.indexOf("OK") >= 0)
        {
            SerialUSB.println("SIM7600 Module had turned on.");
            moduleState = true;
            return moduleState;
        }
        delay(1000);
    }
    return moduleState;
}

void moduleConfigure(){
    //initialize SD Card
    if (!SD.begin(PIN_SD_SELECT)) {
    SerialUSB.println("SD Card initialization failed!");
    while (1);
    }
    SerialUSB.println("SD Card initialization done.");

    //set up for sms & call AT+CMICGAIN=8
    sendData("AT+CNMI = 2,2,0,0,0", 3000, DEBUG);
    sendData("AT+CMGF = 1", 3000, DEBUG);
    sendData("AT+CSMP = 17,167,0,0", 3000, DEBUG);
    sendData("AT+CMICGAIN=8", 3000, DEBUG);
    SerialUSB.println("Maduino Zero Configured!");
    sendText("Bockey Configured~xx",rockeyNum,3000,DEBUG);
}

void reportBatt(){
  
  //calculating Battery level
  int voltageA = int(analogRead(A1)*3.3*2*100/1024); 
  int voltageB = int(analogRead(A1)*3.3*2*10/1024+0.5); 

  //sending to Rockey's number
  const String rockeyNum = "AT+CMGS = \"+16173960660\";";

  //sending
  sendData(rockeyNum, 3000, DEBUG); //text begin
  Serial1.print(String(voltageA)+","+String(voltageB)); //text Content
  sendData("1A", 3000, DEBUG); //text end
}
