// Arduino Wifi Dashboard
// Written by Oli Norwell

// Features
// - Connects via Wifi to a remote server to retrieve status information about a web service (in this case www.whatsglide.com)
// - 3 RGB LED lights
// - 32 x 8 LED Matrix display for showing key information
// - 2 x 7Segement 8Digit LED displays for showing lesser information
// - USB powered, uses 0.3-0.4A at 5V
// - Black wooden custom built box mountable on the wall

// Budget
// Arduino Mega clone (11 euros)
// 20+ DuPont wires (2 euros)
// ESP8266 chip (4.5 euros)
// 5V->3.5V convertor (2 euros)
// Box (4.50 euros)
// 3 RGB LEDS (1 euro)
// 2 8Digit displays (10 euros)
// Matrix display (14 euros)
// Breadboard (3 euros)
// total: 50 euros

// Example feedback from server:   "|$|1|1|0|51|36|2|2|1|"

// Library includes
#include "ESP8266.h"                      // For interfacing with the ESP8266 chip
#include "LedControl.h"                   // LED Control library (http://playground.arduino.cc/Main/LedControl)

// Globals
LedControl display = LedControl(A2,A1,A0,6);  // Our daisy-chained 2 LED displays
SoftwareSerial mySerial(10, 11);              // SoftwareSerial pins for MEGA/Uno. For other boards see: https://www.arduino.cc/en/Reference/SoftwareSerial
ESP8266 wifi(mySerial);                       // Wifi connection data

// Wifi Network Details (we should really load these from an SD card)
const char *SSID     = "PUT WIFI NETWORK HERE";
const char *PASSWORD = "PUT WIFI PASSWORD HERE";

// Defines
#define REFRESH_TIME        60000     // 1 minute
#define INFO_REFRESH_TIME   2000      // 2 seconds

#define SERVER_IP   "192.168.1.117" // could just as easily be an Internet site
#define SERVER_PORT 9999

#define STATUS_LIGHTS
#define DIGITAL_LEDS
//#define SERIAL_DEBUG

// Globals
int gLED_INTENSITY = 15; //15;
int gLED_COLORINT = 150; //255;     // Lower than 150 doesn't seem to work

// LED pins (each light is actually 3 LEDS, allowing us to mix and create any colour)
int red1Pin = A5;
int green1Pin = A4;
int blue1Pin = A3;

int red2Pin = 7;
int green2Pin = 6;
int blue2Pin = 5;

int red3Pin = 4;
int green3Pin = 3;
int blue3Pin = 2;

// Globals to know when we enter / are in night mode   (in night mode we show minimal LEDs - just enough to confirm that all is ok)
int g_oldnightmode = 0;
int g_nightmode = 0;

// Dot-matrix image definitions (0-9 and some icons)    - from https://xantorohara.github.io/led-matrix-editor/
const uint64_t IMAGES[] = {
  0x3c66666e76663c00,
  0x7e1818181c181800,
  0x7e060c3060663c00,
  0x3c66603860663c00,
  0x30307e3234383000,
  0x3c6660603e067e00,
  0x3c66663e06663c00,
  0x1818183030667e00,
  0x3c66663c66663c00,
  0x3c66607c66663c00,
  0xdf51515d4151515f,
  0x20aeaea2beaeaea0,
  0xdd484848484848dc,
  0x22b7b7b7b7b7b723,
  0x390a0a0a3a0a0a39,
  0xc6f5f5f5c5f5f5c6,
  0x49db490077515157,
  0x49db490073555553,
  0x49db49e08fe121ef,
  0x49db49e08fe929e7,
  0x0000b8a8a8a8b800,
  0x0000140211121400
};
const int IMAGES_LEN = sizeof(IMAGES)/8;


// Our bits of data from the remote server - all pre-set to -1, which means no data
int state_s1 = -1;
int state_s2 = -1;
int state_cj = -1;
int state_acu = -1;
int state_acs = -1;
int state_dacu = -1;
int state_dacs = -1;

bool csuccess = false;
bool success = false;
unsigned long millis_last_page_grab;
unsigned long millis_last_view_change;

// Different views
#define VIEW__CURRENT_USERS     0
#define VIEW__DAILY_USERS       1
#define VIEW__CURRENT_SYSTEMS   2
#define VIEW__DAILY_SYSTEMS     3

// Current view
int g_currentView = VIEW__CURRENT_USERS;

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void setup(void)
{

  #ifdef SERIAL_DEBUG     // Start Serial Monitor (but only if we are compiling with serial debugging)
  Serial.begin(9600);
  Serial.println("Glide Dash 001 - Serial Debuging Enabled");
  #endif
  
  #ifdef STATUS_LIGHTS  // Setup status lights (but only if they are activated in this build)
  pinMode(red1Pin, OUTPUT);
  pinMode(green1Pin, OUTPUT);
  pinMode(blue1Pin, OUTPUT); 
  
  pinMode(red2Pin, OUTPUT);
  pinMode(green2Pin, OUTPUT);
  pinMode(blue2Pin, OUTPUT); 
  
  pinMode(red3Pin, OUTPUT);
  pinMode(green3Pin, OUTPUT);
  pinMode(blue3Pin, OUTPUT); 
  #endif

  // Setup our LED display components
  for(int a = 0; a < display.getDeviceCount(); a++)
  {
    display.clearDisplay(a);
    display.shutdown(a, false);
    display.setIntensity(a, gLED_INTENSITY);
  }

  // Place some test data
  display.setChar(4,0,'0',false);
  display.setChar(4,1,'1',false);
  display.setChar(4,2,'2',false);
  display.setChar(4,3,'3',false);
  display.setChar(4,4,'4',false);
  display.setChar(4,5,'5',false);
  display.setChar(4,6,'6',false);
  display.setChar(4,7,'7',false);

  display.setChar(5,0,'0',false);
  display.setChar(5,1,'1',false);
  display.setChar(5,2,'2',false);
  display.setChar(5,3,'3',false);
  display.setChar(5,4,'4',false);
  display.setChar(5,5,'5',false);
  display.setChar(5,6,'6',false);
  display.setChar(5,7,'7',false);


  // Set the lights to blue to show we are connecting to the wifi
  setLEDIndicator(1, gLED_COLORINT, gLED_COLORINT, gLED_COLORINT);  // blue
  setLEDIndicator(2, gLED_COLORINT, gLED_COLORINT, gLED_COLORINT);  // blue
  setLEDIndicator(3, gLED_COLORINT, gLED_COLORINT, gLED_COLORINT);  // blue

  // Setup WiFi
  if (!wifi.init(SSID, PASSWORD, 9600))
  {
    #ifdef SERIAL_DEBUG
        Serial.println("Wifi Init failed. Check configuration.");
    #endif

    setLEDIndicator(1, gLED_COLORINT, gLED_COLORINT, 0);  // error
    setLEDIndicator(2, gLED_COLORINT, gLED_COLORINT, 0);  // error
    setLEDIndicator(3, gLED_COLORINT, gLED_COLORINT, 0);  // error

    // If we have a wifi error - then ask for reset
    displayMatrix(0, 10);
    displayMatrix(1, 10);
    displayMatrix(2, 10);
    displayMatrix(3, 10);

    delay(5000);
    asm volatile ("  jmp 0");     // Reset the program (not ideal but probably just about does the trick)
    
    while (true) ; // loop eternally
  }
  
  // To get us going, grab the data
  getPage();

  return;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void displayMatrix(int disp, int img) // display an image from the IMAGES array (created using https://xantorohara.github.io/led-matrix-editor/)
{
  if(img < 0 || img > IMAGES_LEN) return;   // Return without doing anything if we are out of range

  uint64_t image = IMAGES[img]; // Read in our image from the array

  // Loop through each LED in the matrix (note: we also specify which of our displays we are setting)
  for (int i = 0; i < 8; i++) 
  {
    byte row = (image >> i * 8) & 0xFF;
    
    for (int j = 0; j < 8; j++) 
    {
      display.setLed(disp, i, j, bitRead(row, j));
    }
  }

  return;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void setLEDIndicator(int which, int red, int green, int blue)
{

// NOTE! - I only want full red, or full green, or full blue - so I removed the analog writing which is needed for creating mixed colours! ************************

  #ifdef STATUS_LIGHTS
  
  if(which == 1)
  {
    if(red > 0) digitalWrite(red1Pin, HIGH); else digitalWrite(red1Pin, LOW);
    if(green > 0) digitalWrite(green1Pin, HIGH); else digitalWrite(green1Pin, LOW);
    if(blue > 0) digitalWrite(blue1Pin, HIGH); else digitalWrite(blue1Pin, LOW);
  }
  else if(which == 2)
  {
    if(red > 0) digitalWrite(red2Pin, HIGH); else digitalWrite(red2Pin, LOW);
    if(green > 0) digitalWrite(green2Pin, HIGH); else digitalWrite(green2Pin, LOW);
    if(blue > 0) digitalWrite(blue2Pin, HIGH); else digitalWrite(blue2Pin, LOW);
  }
  else if(which == 3)
  {
    if(red > 0) digitalWrite(red3Pin, HIGH); else digitalWrite(red3Pin, LOW);
    if(green > 0) digitalWrite(green3Pin, HIGH); else digitalWrite(green3Pin, LOW);
    if(blue > 0) digitalWrite(blue3Pin, HIGH); else digitalWrite(blue3Pin, LOW);
  }

  #endif

  return;
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void loop(void)
{   

    // Change the view every 2 seconds
    if(millis() - millis_last_view_change > INFO_REFRESH_TIME)
    {
      millis_last_view_change = millis();

      g_currentView++;
      if(g_currentView > VIEW__DAILY_SYSTEMS) g_currentView = VIEW__CURRENT_USERS; // Reset to our first view

      // Update the view
      updateView(g_currentView);
    }


    // Grab a page every minute
    if(millis() - millis_last_page_grab > REFRESH_TIME)
    {
      millis_last_page_grab = millis();

      #ifdef BLUE_LOADING_LIGHTS
      // set the colours to blue
      setLEDIndicator(1, 0, 0, gLED_COLORINT);
      setLEDIndicator(2, 0, 0, gLED_COLORINT);
      setLEDIndicator(3, 0, 0, gLED_COLORINT);
      #endif

      success = getPage();
    }

  return;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void updateView(int whichView)
{
  if(g_nightmode == 1) return;
  
  int ths = 0;
  int hun = 0;
  int ten = 0;
  int dig = 0;

  if(whichView == VIEW__CURRENT_USERS)
  {
      // Current users (in last 60 seconds)

      // Work out the digit for each column (we have a max of 9999 - more users than that and I'll need to make a new dashboard!)
      ths = state_acu / 1000;
      hun = (state_acu - (ths * 1000)) / 100;
      ten = (state_acu - (ths * 1000) - (hun * 100)) / 10;
      dig = (state_acu - (ths * 1000) - (hun * 100) - (ten * 10));

      // If we have thousands, then show them, otherwise we show an explanation icon
      if(ths != 0)
      {
        displayMatrix(3, ths);
      }
      else
      {
        // our image for the info
        displayMatrix(3, 16);
      }

      // 100s
      if(hun != 0)
      {
        display.shutdown(2, false);
        displayMatrix(2, hun);
      }
      else display.clearDisplay(2); // or blank

      // 10s
      if(ten != 0)
      {
        display.shutdown(1, false);
        displayMatrix(1, ten);
      }
      else display.clearDisplay(1); // or blank

      // 0s (we always display this data)
      displayMatrix(0, dig);

  }
  if(whichView == VIEW__DAILY_USERS)
  {
      // Daily users (since 00:00)

      // Work out the digit for each column (we have a max of 9999 - more users than that and I'll need to make a new dashboard!)
      ths = state_dacu / 1000;
      hun = (state_dacu - (ths * 1000)) / 100;
      ten = (state_dacu - (ths * 1000) - (hun * 100)) / 10;
      dig = (state_dacu - (ths * 1000) - (hun * 100) - (ten * 10));

      // If we have thousands, then show them, otherwise we show an explanation icon
      if(ths != 0)
      {
        displayMatrix(3, ths);
      }
      else
      {
        // our image for the info
        displayMatrix(3, 17);
      }

      // 100s
      if(hun != 0)
      {
        display.shutdown(2, false);
        displayMatrix(2, hun);
      }
      else display.clearDisplay(2); // or blank

      // 10s
      if(ten != 0)
      {
        display.shutdown(1, false);
        displayMatrix(1, ten);
      }
      else display.clearDisplay(1); // or blank

      // 0s (we always display this data)
      displayMatrix(0, dig);

  }
  if(whichView == VIEW__CURRENT_SYSTEMS)
  {
      // Active systems (in last 60 seconds)

      // Work out the digit for each column (we have a max of 9999 - more users than that and I'll need to make a new dashboard!)
      ths = state_acs / 1000;
      hun = (state_acs - (ths * 1000)) / 100;
      ten = (state_acs - (ths * 1000) - (hun * 100)) / 10;
      dig = (state_acs - (ths * 1000) - (hun * 100) - (ten * 10));

      // If we have thousands, then show them, otherwise we show an explanation icon
      if(ths != 0)
      {
        displayMatrix(3, ths);
      }
      else
      {
        // our image for the info
        displayMatrix(3, 18);
      }

      // 100s
      if(hun != 0)
      {
        display.shutdown(2, false);
        displayMatrix(2, hun);
      }
      else display.clearDisplay(2); // or blank

      // 10s
      if(ten != 0)
      {
        display.shutdown(1, false);
        displayMatrix(1, ten);
      }
      else display.clearDisplay(1); // or blank

      // 0s (we always display this data)
      displayMatrix(0, dig);
  }
  if(whichView == VIEW__DAILY_SYSTEMS)
  {
      // Daily Active systems (since 00:00)

      // Work out the digit for each column (we have a max of 9999 - more users than that and I'll need to make a new dashboard!)
      ths = state_dacs / 1000;
      hun = (state_dacs - (ths * 1000)) / 100;
      ten = (state_dacs - (ths * 1000) - (hun * 100)) / 10;
      dig = (state_dacs - (ths * 1000) - (hun * 100) - (ten * 10));

      // If we have thousands, then show them, otherwise we show an explanation icon
      if(ths != 0)
      {
        displayMatrix(3, ths);
      }
      else
      {
        // our image for the info
        displayMatrix(3, 19);
      }

      // 100s
      if(hun != 0)
      {
        display.shutdown(2, false);
        displayMatrix(2, hun);
      }
      else display.clearDisplay(2); // or blank

      // 10s
      if(ten != 0)
      {
        display.shutdown(1, false);
        displayMatrix(1, ten);
      }
      else display.clearDisplay(1); // or blank

      // 0s (we always display this data)
      displayMatrix(0, dig);
  }


return;  
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
int getPage()
{
  millis_last_page_grab = millis(); // update the last time we went off to get a page

  g_oldnightmode = g_nightmode;
  
  // The request
  char* request =  "GET /gp/dbd.php HTTP/1.1\r\nHost: 192.168.1.112\r\nConnection: close\r\n\r\n";

  // Connect to Server
  if (wifi.createTCP(SERVER_IP, SERVER_PORT))
  {

  }
  else
  {
#ifdef SERIAL_DEBUG
    Serial.println(F("create tcp - ERROR"));
#endif
    return "";
  }

  if (!wifi.sendSingle(request))
  {
#ifdef SERIAL_DEBUG
    Serial.print(F("not sent"));
#endif
    return 0;
  }

  int len = wifi.recv(wifi.m_responseBuffer, MAX_BUFFER_SIZE, 1000);

char stats[255];
char* stat = strstr((char*)wifi.m_responseBuffer,"|$|");
char* s2 = stat + 3;
#ifdef SERIAL_DEBUG
Serial.println((char*)s2);
#endif

// Read the data
int cStat = 1; // which current stat
char * pch;
pch = strtok (s2,"|");
state_s1 = atoi(pch);


// ****** So this next bit walks through the response from the server and puts the numbers into variables, the server sends us the data split by | characters
// ****** You could use this same technique in your dashboard, or some other way of seperating the values
// ****** The only key thing is that it is going to be consistent, and that you can interpret what you get from the server reliably


  while (pch != NULL)
  {
#ifdef SERIAL_DEBUG
    Serial.println(pch);
#endif
    pch = strtok (NULL, "|");
    if(cStat == 1) state_s2 = atoi(pch);
    if(cStat == 2) state_cj = atoi(pch);
    if(cStat == 3) state_dacu = atoi(pch);
    if(cStat == 4) state_dacs = atoi(pch);
    if(cStat == 5) state_acu = atoi(pch);
    if(cStat == 6) state_acs = atoi(pch);
    if(cStat == 7) g_nightmode = atoi(pch);
    cStat++;
  }



#ifdef SERIAL_DEBUG
Serial.println("S1: ");
Serial.println(state_s1);

Serial.println("S2: ");
Serial.println(state_s1);

Serial.println("ACU: ");
Serial.println(state_acu);

Serial.println("DACS: ");
Serial.println(state_dacs);
#endif


if(g_nightmode == 1 && state_s1 == 1 && state_s2 == 1) // we have gone into night mode (and both servers are okay)
{
  setLEDIndicator(1, 0, 0, 0);  // off
  setLEDIndicator(2, 0, 0, 0);  // off
  setLEDIndicator(3, 0, 0, 0);  // off

  display.clearDisplay(0);
  display.clearDisplay(3);

  display.shutdown(1, false);
  display.shutdown(2, false);
  displayMatrix(1, 21);
  displayMatrix(2, 20);

  for(int a = 4; a < 6; a++)
  {
    for(int b = 0; b < 8; b++)
    {
      display.setChar(a,b,' ',false);
    }
  }

}
else // NORMAL DAY MODE
{

  // Update the colours of our leds representing server statuses
  if(state_s1 == 1)
  {
    setLEDIndicator(1, 0, gLED_COLORINT, 0);  // red
  }
  else if(state_s1 == 0)
  {
    setLEDIndicator(1, gLED_COLORINT, 0, 0);  // red
  }
  else
  {
    setLEDIndicator(1, 0, 0, gLED_COLORINT);  // red
  }
  
  
  if(state_s2 == 1)
  {
    setLEDIndicator(2, 0, gLED_COLORINT, 0);  // red
  }
  else if(state_s2 == 0)
  {
    setLEDIndicator(2, gLED_COLORINT, 0, 0);  // red
  }
  else
  {
    setLEDIndicator(2, 0, 0, gLED_COLORINT);  // red
  }
  
  
  if(state_cj == 0) // as in okay!
  {
    setLEDIndicator(3, 0, gLED_COLORINT, 0);  // red
  }
  else if(state_cj > 0) // as in not okay!
  {
    setLEDIndicator(3, gLED_COLORINT, 0, 0);  // red
  }
  else // not set
  {
    setLEDIndicator(3, 0, 0, gLED_COLORINT);  // red
  }

  
  
  // ****** this next bit gets us from integer variables like 354, 1343, 23 etc.... to a series of characters that we can then display in their relevant locations
  
  // ***** Note: there are easier ways to do this - I chose this method as it shows clearly what we are doing and works well for a tutorial
  
  int disp = 4;

  int number = state_dacs;
  char res[8]; // 8 max
  for(int a = 0; a < 8; a++)
  {
    res[a] = ' ';
  }

  int ths, hun, ten, dig;

  ths = number / 1000;
  res[0] = '0' + ths;
  if(ths == 0) res[0] = ' ';
  hun = (number- (ths * 1000)) / 100;
  res[1] = '0' + hun;
  if(ths == 0 && hun == 0) res[1] = ' ';
  ten = (number - (ths * 1000) - (hun * 100)) / 10;
  res[2] = '0' + ten;
  if(ths == 0 && hun == 0 && ten == 0) res[2] = ' ';
  dig = (number - (ths * 1000) - (hun * 100) - (ten * 10));
  res[3] = '0' + dig;
  res[4] = 0;  

  display.setChar(disp,7,res[0],false);
  display.setChar(disp,6,res[1],false);
  display.setChar(disp,5,res[2],false);
  display.setChar(disp,4,res[3],false);



  number = state_dacu;
  for(int a = 0; a < 8; a++)
  {
    res[a] = ' ';
  }

  ths = number / 1000;
  res[0] = '0' + ths;
  if(ths == 0) res[0] = ' ';
  hun = (number- (ths * 1000)) / 100;
  res[1] = '0' + hun;
  if(ths == 0 && hun == 0) res[1] = ' ';
  ten = (number - (ths * 1000) - (hun * 100)) / 10;
  res[2] = '0' + ten;
  if(ths == 0 && hun == 0 && ten == 0) res[2] = ' ';
  dig = (number - (ths * 1000) - (hun * 100) - (ten * 10));
  res[3] = '0' + dig;
  res[4] = 0;  

  display.setChar(disp,3,res[0],false);
  display.setChar(disp,2,res[1],false);
  display.setChar(disp,1,res[2],false);
  display.setChar(disp,0,res[3],false);


disp = 5; // last one

  number = state_acu;
  for(int a = 0; a < 8; a++)
  {
    res[a] = ' ';
  }

  ths = number / 1000;
  res[0] = '0' + ths;
  if(ths == 0) res[0] = ' ';
  hun = (number- (ths * 1000)) / 100;
  res[1] = '0' + hun;
  if(ths == 0 && hun == 0) res[1] = ' ';
  ten = (number - (ths * 1000) - (hun * 100)) / 10;
  res[2] = '0' + ten;
  if(ths == 0 && hun == 0 && ten == 0) res[2] = ' ';
  dig = (number - (ths * 1000) - (hun * 100) - (ten * 10));
  res[3] = '0' + dig;
  res[4] = 0;  

  display.setChar(disp,7,res[0],false);
  display.setChar(disp,6,res[1],false);
  display.setChar(disp,5,res[2],false);
  display.setChar(disp,4,res[3],false);


  number = state_acs;
  for(int a = 0; a < 8; a++)
  {
    res[a] = ' ';
  }

  ths = number / 1000;
  res[0] = '0' + ths;
  if(ths == 0) res[0] = ' ';
  hun = (number- (ths * 1000)) / 100;
  res[1] = '0' + hun;
  if(ths == 0 && hun == 0) res[1] = ' ';
  ten = (number - (ths * 1000) - (hun * 100)) / 10;
  res[2] = '0' + ten;
  if(ths == 0 && hun == 0 && ten == 0) res[2] = ' ';
  dig = (number - (ths * 1000) - (hun * 100) - (ten * 10));
  res[3] = '0' + dig;
  res[4] = 0;  

  display.setChar(disp,3,res[0],false);
  display.setChar(disp,2,res[1],false);
  display.setChar(disp,1,res[2],false);
  display.setChar(disp,0,res[3],false);
}

  return len;
}