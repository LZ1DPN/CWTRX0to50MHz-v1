/*
Revision 1.0 - Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - November 6th, 2013...  ever so slight revision by  VK8BN for AD9851 chip Feb 24 2014
Revision 3.0 - April, 2016 - AD9851 + ARDUINO PRO NANO + integrate cw decoder (by LZ1DPN) (uncontinued version)
Revision 4.0 - May 31, 2016  - deintegrate cw decoder and add button for band change (by LZ1DPN)
Revision 5.0 - July 20, 2016  - with LCD display + IF --> ready to control transceiver RFT SEG-100 (by LZ1DPN)
Revision 6.0 - August 16, 2016  - serial control buttons from computer with USB serial (by LZ1DPN) (1 up freq, 2 down freq, 3 step increment change, 4 print state)
									for no_display work with DDS generator
*/

#include <SPI.h>
#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>

//#define OLED_RESET 4
//Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


//#define LOGO16_GLCD_HEIGHT 16 
//#define LOGO16_GLCD_WIDTH  16 
//static const unsigned char PROGMEM logo16_glcd_bmp[] =
//{ B00000000, B11000000,
//  B00000001, B11000000,
//  B00000001, B11000000,
//  B00000011, B11100000,
//  B11110011, B11100000,
//  B11111110, B11111000,
//  B01111110, B11111111,
//  B00110011, B10011111,
//  B00011111, B11111100,
//  B00001101, B01110000,
//  B00011011, B10100000,
//  B00111111, B11100000,
//  B00111111, B11110000,
//  B01111100, B11110000,
//  B01110000, B01110000,
//  B00000000, B00110000 };

//#if (SSD1306_LCDHEIGHT != 32)
//#error("Height incorrect, please fix Adafruit_SSD1306.h!");
//#endif


// Include the library code
#include <LiquidCrystal.h>
#include <rotary.h>
#include <EEPROM.h>

//Setup some items
//#define VFO_VERSION "6.0"

#define W_CLK 8   // Pin 8 - connect to AD9851 module word load clock pin (CLK)
#define FQ_UD 9   // Pin 9 - connect to freq update pin (FQ)
#define DATA 10   // Pin 10 - connect to serial data load pin (DATA)
#define RESET 11  // Pin 11 - connect to reset pin (RST) 
#define BTNDEC (A2)  // BAND CHANGE BUTTON
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
Rotary r = Rotary(2,3); // sets the pins the rotary encoder uses.  Must be interrupt pins.
LiquidCrystal lcd(12, 13, 7, 6, 5, 4); // I used an odd pin combination because I need pin 2 and 3 for the interrupts.

int_fast32_t rx=7000000; // Starting frequency of VFO
int_fast32_t rx2=1; // variable to hold the updated frequency
//int_fast32_t rxif=28200000; // IF freq, they be summ with vfo freq
int_fast32_t rxif=0; // IF freq, they be summ with vfo freq

int_fast32_t increment = 100; // starting VFO update increment in HZ.
int buttonstate = 0;
// {increment = 100;  hertz = "100 Hz"; hertzPosition=4;}
String hertz = "100 Hz";
int  hertzPosition = 0;

byte ones,tens,hundreds,thousands,tenthousands,hundredthousands,millions ;  //Placeholders
String freq; // string to hold the frequency
int_fast32_t timepassed = millis(); // int to hold the arduino miilis since startup
int memstatus = 1;  // value to notify if memory is current or old. 0=old, 1=current.

int ForceFreq = 1;  // Change this to 0 after you upload and run a working sketch to activate the EEPROM memory.  YOU MUST PUT THIS BACK TO 0 AND UPLOAD THE SKETCH AGAIN AFTER STARTING FREQUENCY IS SET!

int byteRead = 0;

const int colums = 16; /// have to be 16 or 20
const int rows = 2;  /// have to be 2 or 4

int lcdindex = 0;
int line1[colums];
int line2[colums];

//int audioInPin = A1;   // both with one GND pin external from case jumper for further use (TRANSMIT/RECEIVE for POWER AMPLIFIER or something ...
int BTNdecodeON = 0;
int BTNlaststate = 0;
int BTNcheck = 0;
int BTNcheck2 = 0;
int BTNinc = 3; // set default band minus 1  ==> (for 7MHz = 3)

void setup() {

// Initialize the Serial port so that we can use it for debugging
  Serial.begin(115200);
  Serial.println("Start VFO ver 6.0");

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
//  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
//  display.display();

  // Clear the buffer.
//  display.clearDisplay();
  
  // text display tests
//  display.setTextSize(2);
//  display.setTextColor(WHITE);
//  display.setCursor(0,0);
//  display.println(rx);
//  display.display();
  
  pinMode(BTNDEC,INPUT);		// temporary use for band change
  digitalWrite(BTNDEC,HIGH);    //
  pinMode(A0,INPUT); // Connect to a button that goes to GND on push - rotary encoder FREQ STEP
  digitalWrite(A0,HIGH);
  lcd.begin(16, 2);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT); 
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode on the AD9851 - see datasheet
  lcd.setCursor(hertzPosition,1);    
  lcd.print(hertz);

//  display.clearDisplay();	
//  display.setCursor(0,0);
//  display.println(rx);
//  display.setCursor(0,18);
//  display.println(hertz);
//  display.display();
  
   // Load the stored frequency  
  if (ForceFreq == 0) {
    freq = String(EEPROM.read(0))+String(EEPROM.read(1))+String(EEPROM.read(2))+String(EEPROM.read(3))+String(EEPROM.read(4))+String(EEPROM.read(5))+String(EEPROM.read(6));
    rx = freq.toInt();  
  }
  
 for (int index = 0; index < colums; index++){
    line1[index] = 32;
	line2[index] = 32;
 }
    
}


///// START LOOP - MAIN LOOP

void loop() {
	checkBTNdecode();

  if (rx != rx2){
		BTNcheck = 0;   
						
		if (BTNcheck == 0) {
			showFreq();
//          display.clearDisplay();	
//			display.setCursor(0,0);
//			display.println(rx);
//			display.setCursor(0,18);
//			display.println(hertz);
//			display.display();

			lcd.setCursor(0,1);
			lcd.print("           ");
			lcd.setCursor(hertzPosition,1); 
			lcd.print(hertz);
		}
        sendFrequency(rx);
        rx2 = rx;
      }
      
  buttonstate = digitalRead(A0);
  if(buttonstate == LOW) {
        setincrement();        
    };

  // Write the frequency to memory if not stored and 2 seconds have passed since the last frequency change.
    if(memstatus == 0){   
      if(timepassed+2000 < millis()){
        storeMEM();
        }
      }   
	  
	  
   /*  check if data has been sent from the computer: */
  if (Serial.available()) {
    /* read the most recent byte */
    byteRead = Serial.read();
	if(byteRead == 49){     // 1 - up freq
		rx = rx + increment;
		}
	if(byteRead == 50){		// 2 - down freq
		rx = rx - increment;
		}
	if(byteRead == 51){		// 3 - up increment
		setincrement();
		}
	if(byteRead == 52){		// 4 - print VFO state
		Serial.println("VFO_VERSION 6.0");
		Serial.println(rx);
		Serial.println(rxif);
		Serial.println(increment);
		Serial.println(hertz);
		}
        if(byteRead == 53){		// 5 - scan freq from 7000 to 7050 and back to 7000
             for (int i=0; i=500; (i=i+100))
                rx = rx + i;
                sendFrequency(rx);
                Serial.println(rx);
                showFreq();
//              display.clearDisplay();	
//				display.setCursor(0,0);
//				display.println(rx);
//				display.setCursor(0,18);
//				display.println(hertz);
//				display.display();
                delay(250);
                }
	}
}	  

///END of main loop ///

/// START INTERNAL FUNCTIONS

ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {    
    if (result == DIR_CW){rx=rx+increment;}
    else {rx=rx-increment;};       
      if (rx >=70000000){rx=rx2;}; // UPPER VFO LIMIT 
      if (rx <=100000){rx=rx2;}; // LOWER VFO LIMIT (org<=1)
  }
}



// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {  
  int32_t freq = (frequency + rxif) * 4294967296./180000000;  // note 180 MHz clock on 9851. also note slight adjustment of this can be made to correct for frequency error of onboard crystal
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x001);   // Final control byte, LSB 1 to enable 6 x xtal multiplier on 9851 set to 0x000 for 9850
  pulseHigh(FQ_UD);  // Done!  Should see output
  
    Serial.println(frequency);
//    Serial.println(frequency + rxif);
}

// transfers a byte, a bit at a time, LSB first to the 9851 via serial DATA line
void tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }

}
void setincrement(){
  if(increment == 1){increment = 10; hertz = "10 Hz"; hertzPosition=0;} 
  else if(increment == 10){increment = 50; hertz = "50 Hz"; hertzPosition=0;}
  else if (increment == 50){increment = 100;  hertz = "100 Hz"; hertzPosition=0;}
  else if (increment == 100){increment = 500; hertz="500 Hz"; hertzPosition=0;}
  else if (increment == 500){increment = 1000; hertz="1 Khz"; hertzPosition=0;}
  else if (increment == 1000){increment = 2500; hertz="2.5 Khz"; hertzPosition=0;}
  else if (increment == 2500){increment = 5000; hertz="5 Khz"; hertzPosition=0;}
  else if (increment == 5000){increment = 10000; hertz="10 Khz"; hertzPosition=0;}
  else if (increment == 10000){increment = 100000; hertz="100 Khz"; hertzPosition=0;}
  else if (increment == 100000){increment = 1000000; hertz="1 Mhz"; hertzPosition=0;} 
  
  else{increment = 1; hertz = "1 Hz"; hertzPosition=0;};  
  lcd.setCursor(0,1);
  lcd.print("           ");
  lcd.setCursor(hertzPosition,1); 
  lcd.print(hertz);
//  display.clearDisplay();	
//  display.setCursor(0,0);
//  display.println(rx);
//  display.setCursor(0,18);
//  display.println(hertz);
//  display.display();
  delay(250); // Adjust this delay to speed up/slow down the button menu scroll speed.
};


void showFreq(){

    millions = int(rx/1000000);
    hundredthousands = ((rx/100000)%10);
    tenthousands = ((rx/10000)%10);
    thousands = ((rx/1000)%10);
    hundreds = ((rx/100)%10);
    tens = ((rx/10)%10);
    ones = ((rx/1)%10);
//  display.clearDisplay();	
//  display.setCursor(0,0);
//  display.println(rx);
//  display.setCursor(0,18);
//  display.println(hertz);
//  display.display();

    lcd.setCursor(0,0);
    lcd.print("                ");
    if (millions > 9){lcd.setCursor(0,0);}
    else{lcd.setCursor(1,0);}
    lcd.print(millions);
    lcd.print(".");
    lcd.print(hundredthousands);
    lcd.print(tenthousands);
    lcd.print(thousands);
    lcd.print(".");
    lcd.print(hundreds);
    lcd.print(tens);
    lcd.print(ones);
    lcd.print(" MHz ");

	timepassed = millis();
    memstatus = 0; // Trigger memory write
};

void storeMEM(){
  //Write each frequency section to a EPROM slot.  Yes, it's cheating but it works!
   EEPROM.write(0,millions);
   EEPROM.write(1,hundredthousands);
   EEPROM.write(2,tenthousands);
   EEPROM.write(3,thousands);
   EEPROM.write(4,hundreds);       
   EEPROM.write(5,tens);
   EEPROM.write(6,ones);   
   memstatus = 1;  // Let program know memory has been written
};


void checkBTNdecode(){

//  BAND CHANGE) !!!
  
BTNdecodeON = digitalRead(BTNDEC);
if(BTNdecodeON != BTNlaststate){
    if(BTNdecodeON == HIGH){
    delay(200);
    BTNcheck2 = 1;
    BTNinc = BTNinc + 1;
switch (BTNinc) {
    case 1:
      rx=1810000;
      break;
    case 2:
      rx=3500000;
      break;
    case 3:
      rx=5250000;
      break;
    case 4:
      rx=7000000;
      break;
    case 5:
      rx=10100000;
      break;
    case 6:
      rx=14000000;
      break;
    case 7:
      rx=18068000;
      break;    
    case 8:
      rx=21000000;
      break;    
    case 9:
      rx=24890000;
      break;    
    case 10:
      rx=28000000;
      break;
    case 11:
      rx=29100000;
      break;    	  
    default:
      if(BTNinc > 11){
         BTNinc = 0;
      }
    break;
  }

//  lcd.clear(); 	//CHECK IF NEED uncoment for cw decode
}

if(BTNdecodeON == LOW){
    BTNcheck2 = 0;
//  lcd.clear();   //CHECK IF NEED uncoment for cw decode
	}
    BTNlaststate = BTNcheck2;
  }
}

//// OK END OF PROGRAM
