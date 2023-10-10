// Bockey: is a solar-powered buoy project installed by Rockey Ke in April, 2023
// 20230505: This version is the first to incorporate different sleep function that allow low power interval on the Maduino board between working sessions.
// Author: Rockey Ke

#include <stdio.h>
#include <string.h>
#include "ArduinoLowPower.h"

//include the SD library:
#include <SPI.h>
#include <SD.h>

// set up variables using the SD utility library 
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


//for serial communication
String from_usb = "";
String incomingString;

//module status
long runTime; //designated run time to differentiate from Millis
bool myModuleState = false;
bool isCalling;
bool isTexting;
bool firstConfig;
long lastConfig;

//for calling and texting
long callStartTime = -1;
int callTimeLimit; //testing: 2 minut call time limit 

const String rockeyNum = "+16173960660";
const String command_textToRockey = "AT+CMGS = \"+16173960660\";";

//battery test
uint16_t battery_voltage;
int battInit;
int battNow;
long battInterval;
long lastBattCheckTime;
long wakeTime;
long bedTime;

//rtc 
bool goodTime;
bool firstTime;
long lastUpdateTime;


void setup()
{
    SerialUSB.begin(115200);
    delay(100);
    Serial1.begin(115200);
    delay(100);

    /* Ensure/ Trouble Shooting Serial Connection
    if (!Serial1){
    Serial1.begin(115200);
    unsigned long now = millis();
      while ((now - millis()) < 2000 && !Serial1)
      {}
    }
    */

    //Serial1.begin(UART_BAUD, SERIAL_8N1, MODEM_RXD, MODEM_TXD);

    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);

    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    delay(100);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(8000);
    //digitalWrite(LTE_PWRKEY_PIN, LOW);

    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW); //Normal Mode
    // digitalWrite(LTE_FLIGHT_PIN, HIGH);//Flight Mode

    //wait till Serial1 turn on
    
    ///////////////////////////////////////////////////////////////////////SETTING
    //battery
    lastBattCheckTime = 0;
    battInterval = 3600000;
    //call
    callTimeLimit = 300000;
    //initializing actions
    myModuleState = true;
    goodTime = false;
    runTime = 0;
    isCalling = 0;
    isTexting = 0;
    lastUpdateTime = 0;
    firstConfig = 0;
    
    sendText("Hello World, I'm Bockey.",rockeyNum,3000,DEBUG);
    moduleConfig();
    reportBatt();
    wakeTime = millis();
    bedTime = 60000;
}

/////////////////////////////////////////////////////////////////////////END OF SETTING


void loop()
{   
    // SerialUSB.println(millis());

    // while (!Serial1){
    // SerialUSB.println("Serial1 not established yet.");
    // Serial1.begin(115200);
    // unsigned long now = millis();
    //   while (((now - millis()) < 2000 )&& !Serial1)
    //   {;}
    // }

    // //bed(); //checking if Bocky need to wake up or sleep

    //to set FIRST RTC time
    if(millis()<86400000 && firstTime == 0){
      send_command_and_wait("AT+CCLK?", "+CCLK", 3000);
      //to-do need to update rtc
      firstTime = 1;
      goodTime = 1;
      lastUpdateTime = millis();
    }

    //to re-configurate module
    if (millis()-lastConfig>360000){
      moduleConfig();
    }

    if (millis()>(lastBattCheckTime+battInterval)){
      lastBattCheckTime = millis();
      reportBatt();
      moduleConfig();
      callTimeLimit = (int(analogRead(A1)*3.3*2*10/1024+0.5)-35)*60000; //update call time limit
    }

    //while (Serial1.available() > 0 || incomingString != "")
    while (Serial1.available() > 0)
    {

        incomingString = Serial1.readStringUntil('\n\r');
        SerialUSB.println(incomingString);

         //answer call if ring
        if (incomingString.indexOf("RING") >0){
           SerialUSB.println("Call Detected");
           sendData("ATA", 3000, DEBUG);
           callStartTime = millis();
           isCalling = 1;
        }
        
        if (incomingString.indexOf("SMS DONE") >0 ||incomingString.indexOf("+SMS FULL") >0){
          delay(100);
          if (!Serial1){
            Serial1.begin(115200);
          } //wait till Serial1 turn on

          moduleConfig();
        }

        incomingString.toLowerCase();

        if (incomingString.indexOf("anxious") >= 0){
           sendText("Hey...It's okay, I'm here. :)",rockeyNum,3000,DEBUG);
           delay(500);
        }

        if (incomingString.indexOf("battery") >= 0){
           reportBatt();
           delay(500);
        }

        if (incomingString.indexOf("rain") >= 0){
           send_command_and_wait("AT+CMICGAIN=3", "OK", 3000);
           lastConfig = millis();
           //sendText("Lowered Volumn to hear the rain drop :)",rockeyNum,3000,DEBUG);
           delay(500);
        }

        if (incomingString.indexOf("sun") >= 0){
           send_command_and_wait("AT+CMICGAIN=8", "OK", 3000);
           lastConfig = millis();
           //sendText("Sun is out! Volumn back to highest value",rockeyNum,3000,DEBUG);
           delay(500);
        }

        if (incomingString.indexOf("thanks") >=  0){
           sendText("You are welcome :)",rockeyNum,3000,DEBUG);
        }

        if (incomingString.indexOf("+CCLK:") >=  0){
           updateRTC();
           firstTime = true;
           goodTime = true;
           lastUpdateTime = millis();
        }
       
        //SerialUSB.write(Serial1.read());
        incomingString = "";
        yield();
    }
   
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
                sendData(from_usb, 1000, DEBUG);
                from_usb = "";
            }
        }
#else
        Serial1.write(SerialUSB.read());
        yield();
#endif
    }
}

//////////////////////////////////////////////////////////////////// Functions Below

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

  String command = "AT+CMGS = \""+String(cellNumber)+"\";";

  //sending
  isTexting = true;
  sendData(command, timeout, DEBUG);
  sendData(content, 0, false);
  //Serial1.print(content); //test to get rid of the extra line
  sendData("1A", timeout, DEBUG);
  isTexting = false;
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

void moduleConfig(){
    //initialize SD Card
    SerialUSB.println("Configuration Begin");

    if (!SD.begin(PIN_SD_SELECT)) {
    SerialUSB.println("SD Card initialization failed!");
    while (1);
    }
    SerialUSB.println("SD Card initialization done.");

    //set up for sms & call
    send_command_and_wait("AT+CNMI = 2,2,0,0,0","OK", 5000);
    send_command_and_wait("AT+CMGF = 1","OK", 5000);
    send_command_and_wait("AT+CSMP = 17,167,0,0", "OK", 5000);
    send_command_and_wait("AT+CMICGAIN=8", "OK", 5000);
    //sendData("AT+CCLK?", 3000, DEBUG);
    SerialUSB.println("Maduino Zero Configured!");
    //sendText("Bockey Configured :)",rockeyNum,5000,DEBUG);
}

void reportBatt(){
  
  //calculating Battery level
  int voltageA = int(analogRead(A1)*3.3*2*100/1024); 
  int voltageB = int(analogRead(A1)*3.3*2*10/1024+0.5); 
  
  int age = 0;
  if (millis()>86400000){
    age = int(millis()/(1000*60*60*24));
  }

  String response = String("I am feeling roughly " + String(voltageB) + ", precisely" + String(voltageA) + "." + " Bockey is " + String(age) + " days old.");

  SerialUSB.print(response);
  //sending
  sendText(String(response),rockeyNum,3000,DEBUG);
}

void updateRTC(){
  //to do
}




bool sendCommandAndWait(String command)
{
    return send_command_and_wait(command, "OK", 1000);
}

bool send_command_and_wait(String command, String desired_response, const int timeout)
{
    bool is_desired_response = false;
    String last_command_response = "";

    Serial1.println(command);

    // Watch for desired response
    //
    unsigned long time = millis();
    while ((time + timeout) > millis() && !is_desired_response)
    {
        while (Serial1.available())
        {
            char c = Serial1.read();
            last_command_response += c;
        }

        is_desired_response = last_command_response.indexOf(desired_response) >= 0;
    }

    // Read any final characters (new lines...)
    //
    while (Serial1.available())
    {
        char c = Serial1.read();
        last_command_response += c;
    }

    // Strip out command and desired response
    //
    last_command_response.replace(command, "");

    if (is_desired_response)
    {
        last_command_response.replace(desired_response, "");
    }

    last_command_response.trim();

    SerialUSB.println("----> " + command + " <----");
    SerialUSB.println(last_command_response);

    return is_desired_response;
}

// wake up and fall asleep things



bool turnOnModule()
    {
      SerialUSB.println("turn on begin");
      if (myModuleState = false) // if it's off, turn it on.
      {
          digitalWrite(LTE_PWRKEY_PIN, LOW);
          delay(500);
          digitalWrite(LTE_PWRKEY_PIN, HIGH);

          for (int i = 0; i < 20; i++)
          {
              if (moduleStateCheck())
              {
                myModuleState = true;
                SerialUSB.println("module turned on.");
                return true;
              }
              delay(500);
          }

          SerialUSB.println("failed to turn on.");
          sendText("Bockey is in a Coma XO",rockeyNum,3000,DEBUG);
          return false;
      }
      SerialUSB.println("module successfully turned on.");
      return true;
    }


bool turnOffModule()
    {
        SerialUSB.println("module off begin.");
        if (moduleStateCheck()) // if it's on, turn it off.
        {
            digitalWrite(LTE_PWRKEY_PIN, LOW);
            delay(500);
            String msg = String("");
            msg = sendData("AT+CPOF", 1000, DEBUG);
            if (msg.indexOf("OK") >= 0)
            {
                SerialUSB.println("module turned off.");
                return true;
            }
        }
        SerialUSB.println("module turned off.");
        return true;
    }


/*  mcuSleep is a alternative sleep function that further lower energy usage

    void mcuSleep(int sleep_time_ms)
    {
        //int adjusted_sleep = pre_sleep(sleep_time_ms);
        if (sleep_time_ms > 0)
        {
          Serial1.flush();
          Serial1.end();

          LowPower.sleep(sleep_time_ms);

          Serial1.begin(115200);
          unsigned long now = millis();
          while ((now - millis()) < 200 && !Serial1)
          {}
        }
    }

void mySleep(int my_sleep_time_ms){
  turnOffModule();
  mcuSleep(my_sleep_time_ms);
  turnOnModule();
}

*/

void bed(){
  
    //only turn off when there is no texting or phone call involved
    if (isTexting == 0 && isCalling ==0 ){
      if ((millis()>(wakeTime+600000)) && moduleStateCheck()){
        SerialUSB.println("Turning Off");
        // turnOffModule();
        if(turnOffModule()){
          myModuleState = false;
          bedTime = millis(); 
        }
      }
    }

    //turning on after enough sleep
    if (millis()>(bedTime+600000) && !moduleStateCheck()){
      SerialUSB.println("Turning On");
      // turnOnModule();
      if(turnOnModule()){
        myModuleState = true;
        Serial1.begin(115200);
        SerialUSB.begin(115200);
        wakeTime = millis(); 
      }
      moduleConfig();
      sendText("Bockey Waked Up!",rockeyNum,3000,DEBUG);
    }
}

// SD Test

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
