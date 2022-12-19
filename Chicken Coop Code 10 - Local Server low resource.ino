
//http://wiki.sunfounder.cc/index.php?title=I2C_LCD2004--link to I2C display information
//ethernet shield uses 10,11,12,13,and 4 for SD card- cannot be used elsewhere
//hook up chiken door button wires to  relay 1 (pin 7). NO and COM
//hook up water heater USB connector through relay 2 (pin 8), to 12V regulator 
//hook up temp sensor yellows to (pin 5), reds to 5V+, blacks to ground, record and reenter addresses on first boot, mark which is which one at a time. must use 4.7 Kohm pullup resistor on signal wire to 5V+.
//connect door powers to 5V power supply
//arduino to 10V power supply via barrel connector
//connect I2C to GND, 5V VCC, and SDA to A4, SCL to A5
//connect hard buttons for teminal to pins as assigned below.  Must use pulldown resistors of 10KOHM on signal side, to ground, per switch, or signals will float and responses will cycle.

//must come first for some reason, values come from BLYNK .  
#define BLYNK_TEMPLATE_ID           "0"
#define BLYNK_DEVICE_NAME           "Chicktroller Local"
#define BLYNK_AUTH_TOKEN            "ENTER AUTH HERE"
#define W5100_CS  10
#define SDCARD_CS 4
#define sensor_resolution 10
#define ONE_WIRE_BUS 5
//#define BLYNK_PRINT Serial             //comment back in to debug BLYNK connection over serial, uses 4% of program memory.

//libraries to include
#include <SPI.h>
#include <Ethernet.h>
#include <BlynkSimpleEthernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

char auth[] = BLYNK_AUTH_TOKEN;
BlynkTimer timer; 

//display initialization
LiquidCrystal_I2C lcd(0x27,20,4);

//temperature sensor stuff for water heater controller
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempC = 0;
// Addresses of 2 DS18B20s, update to actual after first run!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
uint8_t sensor1[8] = { 0x28, 0xFF, 0x6A, 0x74, 0xC3, 0x17, 0x04, 0x16 };  //address of water temp sensor
uint8_t sensor2[8] = { 0x28, 0xFF, 0x99, 0xD9, 0xB3, 0x17, 0x01, 0x1A };  //address of ambient temp sensor
uint8_t sensor3[8] = { 0x28, 0xFF, 0x9D, 0xF4, 0xC0, 0x17, 0x05, 0x82 };


//button control variables..
int dbpin = 2; //momentary switch for inside door open or close...connect to 5V on one side and Pin 3 on other. triggers a subroutine that opens and closes a relay
int dbpin2= 3;//momentary switch for outside door open or close...connect to 5V on one side and Pin 1 on other. triggers a subroutine that opens and closes a relay
int resetpinbut = 6; //momentary switch for hard reset trigger physically...connect to 5V on one side and Pin 6 on other, triggers a subroutine that hard resets
int heaterpin = 9; //momentary switch so the value can be changed by web or terminal....connect to 5V on one side and Pin 9 on other, triggers a subroutine that changes heaterswitch to control or not control the heater relay based upon temp reading
int heaterswitch = 0;//variable assignment for heater on/off only
int heaterstatus = 0;// slave variable to two master switches, electronic and physical, to store virtual on/off switch positions


//reed switches
int IDsensorval= 0;
int IDpin = A2;
int ODsensorval = 0;
int ODpin = A3;
String insidestring = "null";
String outsidestring = "null";

//initializations
int relay_1 = 7; //chicken inside door button relay control pin
int relay_2 = 8; //water heater power relay control pin
int relay_3 = A0; //chicken outside door button relay control pin


//byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x95, 0xDB }; //physical mac address of ethernet sheild for reference

//BLYNK RECEIPT COMMANDS

BLYNK_WRITE(V8)
{ if (param.asInt() == 1){
  doorbutton();
}
 
}
BLYNK_WRITE(V9)
{if (param.asInt() == 1){
  doorbutton2();
}
}
BLYNK_WRITE(V11)
{if (param.asInt() ==1){
  resetcommand();
}
}
BLYNK_WRITE(V12)
{
  if (param.asInt() == 1){
  heatervalchange();
}
}
//end BLYNK receipt commands

void(* resetFunc) (void) = 0;  // declare reset fuction at address 0


//begin setup
void setup() {

  lcd.init();  //initialize the lcd
  lcd.backlight();  //open the backlight

//begin sensors, and set resolutions to defined resolution variable. lower numbers take longer to calculate and can cause crashes with multiple sensors
  sensors.begin();
  sensors.setResolution(sensor1, sensor_resolution); 
  sensors.setResolution(sensor2, sensor_resolution);
  sensors.setResolution(sensor3, sensor_resolution);

  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH); // Deselect the SD card


//output pin setups
  digitalWrite(relay_1, HIGH);
  digitalWrite(relay_2, HIGH);
  digitalWrite(relay_3, HIGH);
  pinMode(relay_1, OUTPUT);
  pinMode(relay_2, OUTPUT);
  pinMode(relay_3, OUTPUT);

//input pin setups
  digitalWrite(IDpin, LOW);
  digitalWrite(ODpin, LOW);
  digitalWrite(resetpinbut, LOW);
  digitalWrite(heaterpin, LOW);
  digitalWrite(dbpin, LOW);
  digitalWrite(dbpin2, LOW);
  pinMode(resetpinbut, INPUT);
  pinMode(heaterpin, INPUT);
  pinMode(dbpin, INPUT);
  pinMode(dbpin2, INPUT);
  pinMode(IDpin, INPUT);
  pinMode(ODpin, INPUT);

 
  Serial.begin(9600);
   
  
  //begin startup prompts and INFO

  lcd.setCursor ( 0, 0 );            // go to the top left corner
  lcd.print("  The Chicktroller v10 "); // write this string on the top row
  lcd.setCursor ( 0, 1 );            // go to the 2nd row
  lcd.print("  ESTABLISHED 2022  "); // pad string with spaces for centering
  lcd.setCursor ( 0, 2 );            // go to the third row
  lcd.print("  NEVER GONNA GIVE  "); // pad with spaces for centering
  lcd.setCursor ( 0, 3 );            // go to the fourth row
  lcd.print("      YOU UP        ");

  delay (2000);
  lcd.clear();

  
  Blynk.begin(auth, IPAddress(X,X,X,X), 8080);
  timer.setInterval(1500L, dataoutput);
  timer.setInterval(1000L, blynkoutputtemps);
  timer.setInterval(5000L, blynkoutputtime);
  timer.setInterval(500L, blynkoutputdoors);
  timer.setInterval(650L, blynkoutputheat);
  }


//end setup

//begin loop-keep clean!!!
void loop(){

buttonchecks();
heatercontroller();
doorfeedback();
timer.run();

{
Blynk.run();
}

}

//end loop




  
// input reaction subroutines
void doorbutton() {
  lcd.clear();
  lcd.setCursor ( 0, 0 );            
  lcd.print("INSIDE DOOR ACTIVATED"); 
  digitalWrite(relay_1, LOW);
  delay(50);
  digitalWrite(relay_1, HIGH);
  lcd.clear();
      }


void doorbutton2() {
  lcd.clear();
  lcd.setCursor ( 0, 0 );            // go to the top left corner
  lcd.print("OUTSIDE DOOR ACTIVATED"); // write this string on the top row
  digitalWrite(relay_3, LOW);
  delay(50);
  digitalWrite(relay_3, HIGH);
  lcd.clear();
}
void resetcommand() { 
    lcd.clear();
    digitalWrite(relay_1, HIGH);
    digitalWrite(relay_2, HIGH);
    digitalWrite(relay_3, HIGH);
    lcd.setCursor ( 0, 0 );            // go to the top left corner
    lcd.print("3..."); // write this string on the top row
    delay(1000);               
    lcd.setCursor ( 0, 0 );            // go to the top left corner
    lcd.print("2..."); // write this string on the top row
    delay(1000);               
    lcd.setCursor ( 0, 0 );            // go to the top left corner
    lcd.print("1..."); // write this string on the top row
    delay(1000);               
    lcd.setCursor ( 0, 0 );            // go to the top left corner
    lcd.print("SYSTEM RESET...BYE!"); // write this string on the top row
    delay(1000);
    lcd.clear();
    resetFunc(); //call reset
}

void heatervalchange() {
  if(heaterswitch == 1) //if on
 { heaterswitch = 0; // turn off
        }
  else{
    if(heaterswitch == 0) 
     { heaterswitch = 1; 
        }
       }
}
//end input reaction subroutines

//Loop checks
void buttonchecks() {
if (digitalRead(resetpinbut) == HIGH) {
    resetcommand();
  }

if (digitalRead(dbpin) == HIGH) {
    doorbutton();
  }

  if (digitalRead(dbpin2) == HIGH) {
    doorbutton2();
  }


if (digitalRead(heaterpin) == HIGH) {
  heatervalchange();
  } 
}

void heatercontroller() {
if (sensors.getTempC(sensor1) <= 1.66 && heaterswitch == 1)  
                    {
                        digitalWrite(relay_2, LOW); // turn on 10W water heater
                         heaterstatus = 1;
                    }
                    else{
                        if(sensors.getTempC(sensor1) >= 2.77 || heaterswitch == 0) 
                        {
                            digitalWrite(relay_2, HIGH);
                            heaterstatus = 0; //turn off 10W water heater
                        }
                    }
}

void doorfeedback() {
  if (digitalRead(IDpin) == HIGH) {
    IDsensorval = 1;
    insidestring = "CLOSED";
    }
    else {
    IDsensorval = 0;
    insidestring = "OPEN";
    }

if (digitalRead(ODpin) == HIGH) {
    ODsensorval = 1;
    outsidestring = "CLOSED";
    }
    else {
    ODsensorval = 0;
    outsidestring = "OPEN";
    }

}
//end loop checks

//timed subroutines using BLYNK timers

void dataoutput() {

    sensors.requestTemperatures();//refresh sensor readings in a timed loop. keep out of the actual loop code, or delays in this function will stack and cause crashing
  //(col,row) 20x4
    lcd.setCursor ( 0, 0 );            // go to the top left corner
    lcd.print("WATER:"); // write this string on the top row
    lcd.setCursor ( 7, 0 );            // go to the 2nd row
    lcd.print(DallasTemperature::toFahrenheit(sensors.getTempC(sensor1))); // pad string with spaces for centering   DallasTemperature::toFahrenheit(sensors.getTempC(sensor1))
    lcd.setCursor ( 13, 0 ); 
    lcd.print("ID:"); // pad with spaces for centering
    lcd.setCursor ( 16, 0 ); 
    lcd.print(IDsensorval);
    lcd.setCursor ( 0, 1 );            // go to the third row
    lcd.print("OUT:"); // pad with spaces for centering
    lcd.setCursor ( 5, 1 );            // go to the fourth row
    lcd.print(DallasTemperature::toFahrenheit(sensors.getTempC(sensor2)));
    lcd.setCursor ( 13, 1 ); 
    lcd.print("t:");
    lcd.setCursor ( 15, 1 );
    lcd.print(millis() / 1000 / 60);
    lcd.setCursor ( 0, 2 );
    lcd.print("COOP:");
    lcd.setCursor ( 6, 2 );
    lcd.print(DallasTemperature::toFahrenheit(sensors.getTempC(sensor3)));
    lcd.setCursor ( 12, 2 ); 
    lcd.print("OD:"); // pad with spaces for centering
    lcd.setCursor ( 15, 2 ); 
    lcd.print(ODsensorval);
    lcd.setCursor ( 0, 3 );
    lcd.print("HEATSW:");
    lcd.setCursor ( 8, 3 );
    lcd.print(heaterswitch);
    lcd.setCursor ( 12, 3 );
    lcd.print("HSTAT:");
    lcd.setCursor ( 18, 3 );
    lcd.print(heaterstatus);
}


void blynkoutputtemps (){
  Blynk.virtualWrite(V4, DallasTemperature::toFahrenheit(sensors.getTempC(sensor1)));
  Blynk.virtualWrite(V5, DallasTemperature::toFahrenheit(sensors.getTempC(sensor2)));
  Blynk.virtualWrite(V6, DallasTemperature::toFahrenheit(sensors.getTempC(sensor3)));
}

void blynkoutputdoors (){
  Blynk.virtualWrite(V17, IDsensorval);
  Blynk.virtualWrite(V18, ODsensorval);
  Blynk.virtualWrite(V15, insidestring);
  Blynk.virtualWrite(V16, outsidestring);
}


void blynkoutputtime (){
   Blynk.virtualWrite(V14, millis() / 1000 / 60); //send uptime in minutes
}

void blynkoutputheat(){
Blynk.virtualWrite(V7, heaterstatus);
Blynk.virtualWrite(V13, heaterswitch);
}

//end timed subroutines
