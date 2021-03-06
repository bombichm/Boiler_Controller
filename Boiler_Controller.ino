// This Arduino sketch operates a boiler circulation system monitored by (10) Dallas ds18b20 temperature sensors
// wired for parasite power.  The sketch includes math functions designed to eliminate the occasional spurious 
// reading, and to average the last two sensor values.

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <avr/wdt.h>
#include "Wire.h"
#define DEBUG 0

//******************** I2C ********************

const int boilerCirc=0x3; //Boiler Circ Pump
const int furnaceCirc=0x4; //Furnace Circ Pump

//******************** LCD ********************
// LCD Define rs, e, d4, d5, d6, d7
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); //Pins used on the LCD for the Arduino Uno

//****************** One Wire ******************
#define ONE_WIRE_BUS 3    // Data wire is plugged into pin 3 on the Arduino with a 330 Ohm pullup resistor to 5V

OneWire oneWire(ONE_WIRE_BUS);    // Setup a oneWire instance to communicate with any OneWire devices

DallasTemperature sensors(&oneWire);    // Pass our oneWire reference to Dallas Temperature. 

// Assign the addresses of the 1-Wire temp sensors.

DeviceAddress TankTop = { 0x28, 0x5C, 0x02, 0x48, 0x06, 0x00, 0x00, 0xE3 };    //Temp of the top of tank two (right)
DeviceAddress MidTankOne = { 0x28, 0xC8, 0x0C, 0x48, 0x06, 0x00, 0x00, 0x2C };    //Temp of the middle of tank one (left)
DeviceAddress MidTankTwo = { 0x28, 0x91, 0x0E, 0x48, 0x06, 0x00, 0x00, 0x17 };    //Temp of the middle of tank two (right)
DeviceAddress TankReturn = { 0x28, 0x02, 0x9C, 0x46, 0x06, 0x00, 0x00, 0xDE };    //Temp of the bottom of tanks
DeviceAddress Boiler = { 0x28, 0x52, 0x16, 0x48, 0x06, 0x00, 0x00, 0x93 };        //Temp of the boiler
DeviceAddress Outdoor = { 0x28, 0x20, 0x16, 0x48, 0x06, 0x00, 0x00, 0x65 };       //Outdoor ambient temp
DeviceAddress BoilerRoom = { 0x28, 0xD9, 0x0A, 0x48, 0x06, 0x00, 0x00, 0xDC };    //Temp inside the boiler room
DeviceAddress House = { 0x28, 0x85, 0x9E, 0x46, 0x06, 0x00, 0x00, 0x32 };         //Temp inside the house
DeviceAddress DHW = { 0x28, 0x33, 0x14, 0x48, 0x06, 0x00, 0x00, 0xE4 };           //Temp of the DHW tank

//Define variable float values (contain decimel points)

float TankTopTempFbase;
float MidTankOneTempFbase;
float MidTankTwoTempFbase;
float TankReturnTempFbase;
float BoilerTempFbase;
float OutdoorTempFbase;
float BoilerRoomTempFbase;
float HouseTempFbase;
float DHWTempFbase;

float TankTopTempF;
float MidTankOneTempF;
float MidTankTwoTempF;
float TankReturnTempF;
float BoilerTempF;
float OutdoorTempF;
float BoilerRoomTempF;
float HouseTempF;
float DHWTempF;

int lowSetpoint;
int medSetpoint;
int highSetpoint;
String readString; 

const byte notassert = 1;
const byte off = 2;
const byte low = 3;
const byte medium = 4;
const byte high = 5;

//###################### Ethernet ######################//
#include <SPI.h>
#include <Ethernet.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02
};

IPAddress ip(192, 168, 1, 126);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup()
{
  wdt_enable(WDTO_8S);                                            //8 second watchdog timer
  
  Wire.begin();
  Wire.beginTransmission(boilerCirc);
  Wire.write(2);
  Wire.endTransmission();
  
  //###################### Ethernet ######################//
  // start the Ethernet connection and the server:
    Ethernet.begin(mac, ip);
    server.begin();
      
  //###################### Sensors ######################//
    sensors.begin();                                                  // Start up the library
    byte bitRes = 10;                                                 // set the resolution 9-12 bit; 9 bit, 65 ms per chip; 12 bit, 750 ms per chip
    sensors.setResolution(TankTop, bitRes);
    sensors.setResolution(MidTankOne, bitRes);
  //  sensors.setResolution(MidTankTwo, bitRes);
    sensors.setResolution(TankReturn, bitRes);
    sensors.setResolution(Boiler, bitRes);
  //  sensors.setResolution(Outdoor, bitRes);
  //  sensors.setResolution(BoilerRoom, bitRes);
  //  sensors.setResolution(House, bitRes);
  //  sensors.setResolution(DHW, bitRes);
    
    sensors.requestTemperatures();                                    //Get baseline temps
  
  //###################### LCD ######################//
   #if DEBUG == 1
    Serial.begin(9600);
    Serial.println("Assert on, pump off");
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
   
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("BOILER SYSTEM");
    lcd.setCursor(0, 1);
    lcd.print("CONTROLLER");
    delay(1000);
    lcd.clear(); 
   #endif
  
  //###################### Baseline Temps ######################//
    
    getBoilerBaseTempsF();//Calculate baseline temperatures
    lowSetpoint = 168;
    medSetpoint = 174;
    highSetpoint = 180;
 
}//end void setup()

byte pumpSetting;
float maxBoilerTempF;

void loop()
{
  wdt_reset();
  #if DEBUG == 1
    lcdPrintTemps();
    serialPrintTemps();
  #endif

  getBoilerTempsF();
  //MAX Boiler Temp
  if (BoilerTempF > maxBoilerTempF || ((BoilerTempF - maxBoilerTempF) < 2.00))  //This will eliminate a spurious reading from skewing the record
  {
    maxBoilerTempF = BoilerTempF;
  }

  //###################### Ethernet ######################//

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) 
    {
//      Serial.println("new client");
//      // an http request ends with a blank line
//      boolean currentLineIsBlank = true;
      while (client.connected()) 
        {
        if (client.available()) 
          {
          char c = client.read(); 
          //read char by char HTTP request
          if (readString.length() < 100) 
            {
              //store characters to string 
              readString += c; 
              //Serial.print(c);
            }

        //if HTTP request has ended
        if (c == '\n') 
          {

          ///////////////
          Serial.println(readString); //print to serial monitor for debuging 

          //now output HTML data header
             if(readString.indexOf('?') >= 0) 
               { //don't send new page
                 client.println("HTTP/1.1 204 Zoomkat");
                 client.println();
                 client.println();  
               }
          
              else
                {
                // send a standard http response header
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println("Connection: close");  // the connection will be closed after completion of the response
                client.println("Refresh: 5");  // refresh the page automatically every 5 sec
                client.println();
                client.println("<!DOCTYPE HTML>");
                client.println("<html>");
                client.println("<head>");
                client.println("<title>Boiler Controller</title>");
                client.println("</head>");
                client.println("<p style=""font-size:40px"">");
                // output the value of each temp sensor
                client.print("LOW Setpoint = ");
                client.println(lowSetpoint);
                client.println("<a href=\"/?lowup\"\">UP</a>"); 
                client.println("<a href=\"/?lowdown\"\">DOWN</a>"); 
                client.println("<br />");
                client.print("MED Setpoint = ");
                client.println(medSetpoint);
                client.println("<a href=\"/?medup\"\">UP</a>"); 
                client.println("<a href=\"/?meddown\"\">DOWN</a>"); 
                client.println("<br />");
                client.print("HIGH Setpoint = ");
                client.println(highSetpoint);
                client.println("<a href=\"/?highup\"\">UP</a>"); 
                client.println("<a href=\"/?highdown\"\">DOWN</a>"); 
                client.println("<br />");
                client.print("BoilerTempF = ");
                client.println(BoilerTempF);
                client.println("<br />");
                client.print("MaxBoilerTempF = ");
                client.println(maxBoilerTempF);
                client.println("<a href=\"/?resetmax\"\">RESET</a>"); 
                client.println("<br />");
                client.print("TankReturnTempF = ");
                client.println(TankReturnTempF);
                client.println("<br />");
                if (pumpSetting == 1)
                  {
                    client.println("Boiler has control");
                  }
                else if (pumpSetting == 2)
                  {
                    client.println("Pump is off");
                  }
                else if (pumpSetting == 3)
                  {
                    client.println("Pump set to LOW");
                  }
                else if (pumpSetting == 4)
                  {
                    client.println("Pump set to MEDIUM");
                  }
                else if (pumpSetting == 5)
                  {
                    client.println("Pump set to HIGH");
                  }
                client.println("<br />");
                client.print("Up Time (mm:ss) = ");
                unsigned long minutes = millis() /60000;
                byte seconds = (millis() - (minutes * 60000)) /1000; if (seconds >> 60){seconds = 0;}
                client.print(minutes);
                client.print(":");
                client.print(seconds);
                client.println("<br />");
                client.println("</p>");
                client.println("</html>");
                }//end else
      
            // give the web browser time to receive the data
            delay(1);
            // close the connection:
            client.stop();

            ///control from web input
            if(readString.indexOf("lowup") >0)//checks for up
            {
              lowSetpoint += 2;
            }
            if(readString.indexOf("lowdown") >0)//checks for down
            {
              lowSetpoint -= 2;
            }
            if(readString.indexOf("medup") >0)//checks for up
            {
              medSetpoint += 2;
            }
            if(readString.indexOf("meddown") >0)//checks for down
            {
              medSetpoint -= 2;
            }
            if(readString.indexOf("highup") >0)//checks for up
            {
              highSetpoint += 2;
            }
            if(readString.indexOf("highdown") >0)//checks for down
            {
              highSetpoint -= 2;
            }
            if(readString.indexOf("resetmax") >0)//request to reset the max boiler temp
            {
              maxBoilerTempF = BoilerTempF;
            }
            //clearing string for next read
            readString="";

            Serial.println("client disconnected");
          }//if (c == '\n')
        }//end if client
      }//end while connected
    }//end if client
    
  
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////////


///// Conditional tests for setting the boiler circulation pump speed
  
  if (BoilerTempF >= 195.0)                     
  {                                             //This function runs the boiler circ pump on HIGH regardless                                                         
      boilerCircSetting(high);         //of other temps when the boiler is over 195 degrees                                                          
  }  
  
  else if (TankReturnTempF <= 130.0 && BoilerTempF >= lowSetpoint && BoilerTempF <= (medSetpoint - 0.001))   
  {                                             //This function runs the boiler circ pump on LOW when the                                                                   
      boilerCircSetting(low);         //boiler is above 168 degrees                                
  }                                           
                                                
 else if (TankReturnTempF <= 130.0 && BoilerTempF >= (medSetpoint) && BoilerTempF <= (highSetpoint - 0.001))   
  {                                             //This function runs the boiler circ pump on MEDIUM when the                                                                   
      boilerCircSetting(medium);         //boiler is between 174-180 degrees                                
  }                                                                             
    
 else if (TankReturnTempF <= 130.0 && BoilerTempF >= highSetpoint)   
  {                                             //This function runs the boiler circ pump on HIGH when the                                                                   
      boilerCircSetting(high);         //boiler is above 180 degrees and the tank return is less than 130                               
  } 
  
  else if (TankReturnTempF >= 130.0 && BoilerTempF >= medSetpoint && ((BoilerTempF - TankTopTempF) >= 2.0))  
  {                                             //This function runs the boiler circ pump on LOW when the tank                                                                    
      boilerCircSetting(low);         //return is over 130 degrees, and the Boiler Temp is at least 2                              
  }                                             //degrees warmer than the Top of the Tank

  else if (BoilerTempF == -196.60)
  {                                             //This function shuts the circ pump off if the above parameters                           
      boilerCircSetting(notassert);         //are not met
  }
  
  else 
  {                                             //This function shuts the circ pump off if the above parameters                           
      boilerCircSetting(off);         //are not met
  }
           
///// Conditional test for activating the DHW circulation pump  
//  DHWcircON = digitalRead(DHWcirc);
//  
//  if (TankTopTempF >= 155.0)
//  {   digitalWrite(DHWcirc, HIGH);             //This function turns on the DHW circ pump when the water temp is
//                                               //below 102 F and the boiler tanks are above 160 F
//  }  
//     else {
//     digitalWrite(DHWcirc, LOW);
//     
//         }
  

}

byte timer;
void boilerCircSetting(byte data)
{
  //if statement to see how long ago pump changed state
  //variable to store last time state was changed
  
  if (timer >= 30)
    {
    Wire.beginTransmission(boilerCirc);
    Wire.write(data);
    Wire.endTransmission();
    timer = 0;
    #if DATALOG
    dataLog();
    #endif
    }
  else 
    {
      timer++;
      Serial.print ("timer = ");
      Serial.println(timer);
      lcd.setCursor(14, 0);          //New line of the LCD display
      lcd.print(timer);
    }
  pumpSetting = data;
  }

void furnaceCircSetting(byte data)
{
  Wire.beginTransmission(furnaceCirc);
  Wire.write(data);
  Wire.endTransmission();
}

void getBoilerBaseTempsF()
{
  TankTopTempFbase = sensors.getTempF(TankTop);          //Get Top of Tank #2 temp in F
  MidTankOneTempFbase = sensors.getTempF(MidTankOne);    //Get Mid Tank #1 temp in F
  MidTankTwoTempFbase = sensors.getTempF(MidTankTwo);    //Get Mid Tank #2 temp in F
  TankReturnTempFbase = sensors.getTempF(TankReturn);    //Get Tank Return temp in F
  BoilerTempFbase = sensors.getTempF(Boiler);            //Get Boiler temp in F
  OutdoorTempFbase = sensors.getTempF(Outdoor);          //Get Outdoor temp in F
  BoilerRoomTempFbase = sensors.getTempF(BoilerRoom);    //Get Boiler room temp in F
  HouseTempFbase = sensors.getTempF(House);              //Get House temp in F
  DHWTempFbase = sensors.getTempF(DHW);                  //Get DHW temp in F
}

void getBoilerTempsF()
{
  long getTempTime = millis();
  sensors.requestTemperatures();
  Serial.print ("Get Temperatures delay = ");
  Serial.print (millis()-getTempTime);
  Serial.println(" ms");                                     
//  delay(2250);                                  //750 ms per device
  
  TankTopTempF = sensors.getTempF(TankTop);    //Get Top of Tank #2 temp in F
  MidTankOneTempF = sensors.getTempF(MidTankOne);    //Get Mid Tank #1 temp in F
  MidTankTwoTempF = sensors.getTempF(MidTankTwo);    //Get Mid Tank #2 temp in F
  TankReturnTempF = sensors.getTempF(TankReturn);    //Get Tank Return temp in F
  BoilerTempF = sensors.getTempF(Boiler);            //Get Boiler temp in F
  OutdoorTempF = sensors.getTempF(Outdoor);          //Get Outdoor temp in F
  BoilerRoomTempF = sensors.getTempF(BoilerRoom);    //Get Boiler room temp in F
  HouseTempF = sensors.getTempF(House);              //Get House temp in F
  DHWTempF = sensors.getTempF(DHW);                  //Get DHW temp in F
}

//Serial Port Printouts
void serialPrintTemps()
{
  Serial.print("TankTopTempF    ");
  Serial.println(TankTopTempF);
  
  Serial.print("MidTankOneTempF ");
  Serial.println(MidTankOneTempF);
  
//  Serial.print("MidTankTwoTempF ");
//  Serial.println(MidTankTwoTempF);
  
  Serial.print("TankReturnTempF ");
  Serial.println(TankReturnTempF);
  
  Serial.print("BoilerTempF     ");
  Serial.println(BoilerTempF);
  
//  Serial.print("OutdoorTempF    ");
//  Serial.println(OutdoorTempF);
//  
  Serial.print("BoilerRoomTempF ");
  Serial.println(BoilerRoomTempF);
  
//  Serial.print("HouseTempF ");
//  Serial.print(HouseTempF);
  
//  Serial.print("DHWTempF        ");
//  Serial.println(DHWTempF);
  
  Serial.println(" ");
  Serial.print ("Pump Setting is ");
  Serial.println (pumpSetting);
}

void lcdPrintTemps()
{
  /////////////////////////////////////////////////////////////////////////////////////////////////////////
/////LCD Printouts

 
//  lcd.setCursor(0, 0);          //New line of the LCD display (position, line)
//  lcd.print("Top ");  //Prints Tank Top Temperature on LCD
//  lcd.print((int)TankTopTempF);
//  lcd.print("  ");
     
//  lcd.setCursor(8, 0);          //New line of the LCD display
//  lcd.print(" Mid ");
//  lcd.print((int)MidTankOneTempF);
//  lcd.print("  ");                                                     
    
  lcd.setCursor(0, 1);          //New line of the LCD display
  lcd.print("Return ");
  lcd.setCursor(8, 1);
  lcd.print((int)TankReturnTempF);
  
  lcd.setCursor(0, 0);          //New line of the LCD display
  lcd.print("Boiler ");
  lcd.setCursor(8, 0);
  lcd.print((int)BoilerTempF);
  lcd.print("  ");

//  lcd.setCursor(0, 1);          //New line of the LCD display
//  lcd.print("Brm ");
//  lcd.print((int)BoilerRoomTempF);
//  lcd.print("  "); 
//  lcd.setCursor(0, 1);          //New line of the LCD display
//  lcd.print("Outdoor      ");
//  lcd.print((int)OutdoorTempF);
//    delay(3000);
////  
//  lcd.clear();  
//  lcd.setCursor(0, 0);          //New line of the LCD display (position, line)
//  lcd.print("DHW         ");  //Prints Tank Top Temperature on LCD
//  lcd.print((int)DHWTempF);
//     
//  lcd.setCursor(0, 1);          //New line of the LCD display
//  lcd.print("House Temp  ");
//  lcd.print((int)HouseTempF);
//    delay(3000);
//    
    
}

void spuriousTest()
{
  //Spurious readings test, if the reading is 185, 32, or -196, then it will be disregarded
 

////// 
if ((TankTopTempF == 185.0 || TankTopTempF == 32.0 || TankTopTempF == -196.60))                  
{  TankTopTempF = TankTopTempFbase;   }           // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  TankTopTempFbase = ((TankTopTempF + TankTopTempFbase) / 2.0);   
   TankTopTempF = TankTopTempFbase;   }


//////
if ((MidTankOneTempF == 185.0 || MidTankOneTempF == 32.0 || MidTankOneTempF == -196.6))                  
{  MidTankOneTempF = MidTankOneTempFbase;   }           // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  MidTankOneTempFbase = ((MidTankOneTempF + MidTankOneTempFbase) / 2.0);   
   MidTankOneTempF = MidTankOneTempFbase;   }


//////
if ((MidTankTwoTempF == 185.0 || MidTankTwoTempF == 32.0 || MidTankTwoTempF == -196.6))                  
{  MidTankTwoTempF = MidTankTwoTempFbase;   }           // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  MidTankTwoTempFbase = ((MidTankTwoTempF + MidTankTwoTempFbase) / 2.0);   
   MidTankTwoTempF = MidTankTwoTempFbase;   }


//////
if ((TankReturnTempF == 185.0 || TankReturnTempF == 32.0 || TankReturnTempF == -196.6))                  
{  TankReturnTempF = TankReturnTempFbase;   }           // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  TankReturnTempFbase = ((TankReturnTempF + TankReturnTempFbase) / 2.0);   
   TankReturnTempF = TankReturnTempFbase;   }


//////
if ((BoilerTempF == 185.0 || BoilerTempF == 32.0 || BoilerTempF == -196.6))                  
{  BoilerTempF = BoilerTempFbase;   }                   // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  BoilerTempFbase = ((BoilerTempF + BoilerTempFbase) / 2.0);   
   BoilerTempF = BoilerTempFbase;   }


//////
if ((OutdoorTempF == 185.0 || OutdoorTempF == 32.0 || OutdoorTempF == -196.6))                  
{  OutdoorTempF = OutdoorTempFbase;   }                 // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  OutdoorTempFbase = ((OutdoorTempF + OutdoorTempFbase) / 2.0);   
   OutdoorTempF = OutdoorTempFbase;   }


//////
if ((BoilerRoomTempF == 185.0 || BoilerRoomTempF == 32.0 || BoilerRoomTempF == -196.6))                  
{  BoilerRoomTempF = BoilerRoomTempFbase;   }           // The value will revert to the base, otherwise               
                                                        // it will be a running average of the two
else
{  BoilerRoomTempFbase = ((BoilerRoomTempF + BoilerRoomTempFbase) / 2.0);   
   BoilerRoomTempF = BoilerRoomTempFbase;   }


}

