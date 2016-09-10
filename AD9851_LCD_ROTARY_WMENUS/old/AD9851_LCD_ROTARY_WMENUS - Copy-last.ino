/*
Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - November 6th, 2013...  ever so slight revision by  VK8BN for AD9851 chip Feb 24 2014
Revision 4.0 - May 31, 2016  - with cw decoder (now stopped in source, but working well), and button for band change (by LZ1DPN)
*/

// Include the library code
#include <LiquidCrystal.h>
#include <rotary.h>
#include <EEPROM.h>

//Setup some items
//#define VFO_VERSION "1.0"

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

int_fast32_t increment = 100; // starting VFO update increment in HZ.
int buttonstate = 0;
// {increment = 100;  hertz = "100 Hz"; hertzPosition=4;}
String hertz = "100 Hz";
int  hertzPosition = 4;

byte ones,tens,hundreds,thousands,tenthousands,hundredthousands,millions ;  //Placeholders
String freq; // string to hold the frequency
int_fast32_t timepassed = millis(); // int to hold the arduino miilis since startup
int memstatus = 1;  // value to notify if memory is current or old. 0=old, 1=current.

int ForceFreq = 1;  // Change this to 0 after you upload and run a working sketch to activate the EEPROM memory.  YOU MUST PUT THIS BACK TO 0 AND UPLOAD THE SKETCH AGAIN AFTER STARTING FREQUENCY IS SET!


//

///////////////////////////////////////////////////////////////////////
// CW Decoder made by Hjalmar Skovholm Hansen OZ1JHM  VER 1.01       //
// Feel free to change, copy or what ever you like but respect       //
// that license is http://www.gnu.org/copyleft/gpl.html              //
// Discuss and give great ideas on                                   //
// https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages //
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Read more here http://en.wikipedia.org/wiki/Goertzel_algorithm        //
// if you want to know about FFT the http://www.dspguide.com/pdfbook.htm //
///////////////////////////////////////////////////////////////////////////

const int colums = 16; /// have to be 16 or 20
const int rows = 2;  /// have to be 2 or 4

int lcdindex = 0;
int line1[colums];
int line2[colums];

////////////////////////////////
// Define 8 specials letters  //
////////////////////////////////

byte U_umlaut[8] =   {B01010,B00000,B10001,B10001,B10001,B10001,B01110,B00000}; // '�'  
byte O_umlaut[8] =   {B01010,B00000,B01110,B10001,B10001,B10001,B01110,B00000}; // '�'  
byte A_umlaut[8] =   {B01010,B00000,B01110,B10001,B11111,B10001,B10001,B00000}; // '�'    
byte AE_capital[8] = {B01111,B10100,B10100,B11110,B10100,B10100,B10111,B00000}; // '�' 
byte OE_capital[8] = {B00001,B01110,B10011,B10101,B11001,B01110,B10000,B00000}; // '�' 
byte fullblock[8] =  {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};  
byte AA_capital[8] = {B00100,B00000,B01110,B10001,B11111,B10001,B10001,B00000}; // '�'   
byte emtyblock[8] =  {B00000,B00000,B00000,B00000,B00000,B00000,B00000,B00000};  

int audioInPin = A1;   // both with one GND pin external from case jumper for further use (TRANSMIT/RECEIVE for POWER AMPLIFIER or something ...
int BTNdecodeON = 0;
int BTNlaststate = 0;
int BTNcheck = 0;
int BTNcheck2 = 0;
int BTNinc = 4;

float magnitude ;
int magnitudelimit = 100;
int magnitudelimit_low = 100;
int realstate = LOW;
int realstatebefore = LOW;
int filteredstate = LOW;
int filteredstatebefore = LOW;


///////////////////////////////////////////////////////////
// The sampling frq will be 8928 on a 16 mhz             //
// without any prescaler etc                             //
// because we need the tone in the center of the bins    //
// you can set the tone to 496, 558, 744 or 992          //
// then n the number of samples which give the bandwidth //
// can be (8928 / tone) * 1 or 2 or 3 or 4 etc           //
// init is 8928/558 = 16 *4 = 64 samples                 //
// try to take n = 96 or 128 ;o)                         //
// 48 will give you a bandwidth around 186 hz            //
// 64 will give you a bandwidth around 140 hz            //
// 96 will give you a bandwidth around 94 hz             //
// 128 will give you a bandwidth around 70 hz            //
// BUT remember that high n take a lot of time           //
// so you have to find the compromice - i use 48         //
///////////////////////////////////////////////////////////

float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;  
float sampling_freq=8928.0;
float target_freq=558.0; /// adjust for your needs see above
float n=48.0;  //// if you change  her please change next line also 
int testData[48];

//////////////////////////////
// Noise Blanker time which //
// shall be computed so     //
// this is initial          //
//////////////////////////////
int nbtime = 6;  /// ms noise blanker         

long starttimehigh;
long highduration;
long lasthighduration;
long hightimesavg;
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;

char code[20];
int stop = LOW;
int wpm;

//


void setup() {

// Initialize the Serial port so that we can use it for debugging
  Serial.begin(115200);
  Serial.println("Start VFO ver 1.0");

  pinMode(BTNDEC,INPUT);		// temporary use for band change, but originaly for enable CW decoder
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
   // Load the stored frequency  
  if (ForceFreq == 0) {
    freq = String(EEPROM.read(0))+String(EEPROM.read(1))+String(EEPROM.read(2))+String(EEPROM.read(3))+String(EEPROM.read(4))+String(EEPROM.read(5))+String(EEPROM.read(6));
    rx = freq.toInt();  
  }
  
  //
  ////////////////////////////////////
// The basic goertzel calculation //
////////////////////////////////////
  int	k;
  float	omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;

///////////////////////////////
// define special characters //
///////////////////////////////
 lcd.createChar(0, U_umlaut); //     German
 lcd.createChar(1, O_umlaut); //     German, Swedish
 lcd.createChar(2, A_umlaut); //     German, Swedish 
 lcd.createChar(3, AE_capital); //   Danish, Norwegian
 lcd.createChar(4, OE_capital); //   Danish, Norwegian
 lcd.createChar(5, fullblock);        
 lcd.createChar(6, AA_capital); //   Danish, Norwegian, Swedish
 lcd.createChar(7, emtyblock); 
 lcd.clear(); 
 
 for (int index = 0; index < colums; index++){
    line1[index] = 32;
	line2[index] = 32;
 }
  
  //
  
  
}


void loop() {
	checkBTNdecode();

  if (rx != rx2){
		BTNcheck = 0;   // iztrij tova za da raboti CW dekodera !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
						// vazmojno iz sorsa i na drugi mesta da sam pipnal nesto , za da spra CW decodera - nepomnia veche - triabva proverka !!!
		if (BTNcheck == 0) {
			 showFreq();
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
	  
	  ///
	  ///////////////
// main loop //
///////////////

 if(BTNcheck == 1){

  ///////////////////////////////////// 
  // The basic where we get the tone //
  /////////////////////////////////////
  
  for (char index = 0; index < n; index++)
  {
    testData[index] = analogRead(audioInPin);
  }
  for (char index = 0; index < n; index++){
	  float Q0;
	  Q0 = coeff * Q1 - Q2 + (float) testData[index];
	  Q2 = Q1;
	  Q1 = Q0;	
  }
  float magnitudeSquared = (Q1*Q1)+(Q2*Q2)-Q1*Q2*coeff;  // we do only need the real part //
  magnitude = sqrt(magnitudeSquared);
  Q2 = 0;
  Q1 = 0;
  
  /////////////////////////////////////////////////////////// 
  // here we will try to set the magnitude limit automatic //
  ///////////////////////////////////////////////////////////
  
  if (magnitude > magnitudelimit_low){
    magnitudelimit = (magnitudelimit +((magnitude - magnitudelimit)/6));  /// moving average filter
  }
 
  if (magnitudelimit < magnitudelimit_low)
	magnitudelimit = magnitudelimit_low;
  
  ////////////////////////////////////
  // now we check for the magnitude //
  ////////////////////////////////////

  if(magnitude > magnitudelimit*0.6) // just to have some space up 
     realstate = HIGH; 
  else
    realstate = LOW; 
  
  ///////////////////////////////////////////////////// 
  // here we clean up the state with a noise blanker //
  /////////////////////////////////////////////////////
 
  if (realstate != realstatebefore){
	laststarttime = millis();
  }
  if ((millis()-laststarttime)> nbtime){
	if (realstate != filteredstate){
		filteredstate = realstate;
	}
  }
 
 ////////////////////////////////////////////////////////////
 // Then we do want to have some durations on high and low //
 ////////////////////////////////////////////////////////////
 
 if (filteredstate != filteredstatebefore){
	if (filteredstate == HIGH){
		starttimehigh = millis();
		lowduration = (millis() - startttimelow);
	}

	if (filteredstate == LOW){
		startttimelow = millis();
		highduration = (millis() - starttimehigh);
        if (highduration < (2*hightimesavg) || hightimesavg == 0){
			hightimesavg = (highduration+hightimesavg+hightimesavg)/3;     // now we know avg dit time ( rolling 3 avg)
		}
		if (highduration > (5*hightimesavg) ){
			hightimesavg = highduration+hightimesavg;     // if speed decrease fast ..
		}
	}
  }

 ///////////////////////////////////////////////////////////////
 // now we will check which kind of baud we have - dit or dah //
 // and what kind of pause we do have 1 - 3 or 7 pause        //
 // we think that hightimeavg = 1 bit                         //
 ///////////////////////////////////////////////////////////////
 
 if (filteredstate != filteredstatebefore){
  stop = LOW;
  if (filteredstate == LOW){  //// we did end a HIGH
   if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){ /// 0.6 filter out false dits
	strcat(code,".");
	Serial.print(".");
   }
   if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){ 
	strcat(code,"-");
	Serial.print("-");
	wpm = (wpm + (1200/((highduration)/3)))/2;  //// the most precise we can do ;o)
   }
  }
 
   if (filteredstate == HIGH){  //// we did end a LOW
   
   float lacktime = 1;
   if(wpm > 25)lacktime=1.0; ///  when high speeds we have to have a little more pause before new letter or new word 
   if(wpm > 30)lacktime=1.2;
   if(wpm > 35)lacktime=1.5;
   
   if (lowduration > (hightimesavg*(2*lacktime)) && lowduration < hightimesavg*(5*lacktime)){ // letter space
    docode();
	code[0] = '\0';
	Serial.print("/");
   }
   if (lowduration >= hightimesavg*(5*lacktime)){ // word space
    docode();
	code[0] = '\0';
	printascii(32);
	Serial.println();
   }
  }
 }
 
 //////////////////////////////
 // write if no more letters //
 //////////////////////////////

  if ((millis() - startttimelow) > (highduration * 6) && stop == LOW){
   docode();
   code[0] = '\0';
   stop = HIGH;
  }

 /////////////////////////////////////
 // we will turn on and off the LED //
 // and the speaker                 //
 /////////////////////////////////////
 
 // this is for a mod to monitor decoder with led and external tone, but not need for cw decoder to work 
 
   if(filteredstate == HIGH){ 
//     digitalWrite(ledPin, HIGH);
//	 tone(audioOutPin,target_freq);   // this is for a mod to monitor decoder with led and external tone, but not need for cw decoder to work 
   }
   else{
//     digitalWrite(ledPin, LOW);
//	 noTone(audioOutPin);
   }
 
 //////////////////////////////////
 // the end of main loop clean up//
 /////////////////////////////////
 updateinfolinelcd();
 realstatebefore = realstate;
 lasthighduration = highduration;
 filteredstatebefore = filteredstate;
 }
	  ///
	  
}

///END of main loop ///

ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {    
    if (result == DIR_CW){rx=rx+increment;}
    else {rx=rx-increment;};       
      if (rx >=30000000){rx=rx2;}; // UPPER VFO LIMIT 
      if (rx <=1000000){rx=rx2;}; // LOWER VFO LIMIT (org<=1)
  }
}



// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {  
  int32_t freq = frequency * 4294967296./180000000;  // note 180 MHz clock on 9851. also note slight adjustment of this can be made to correct for frequency error of onboard crystal
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x001);   // Final control byte, LSB 1 to enable 6 x xtal multiplier on 9851 set to 0x000 for 9850
  pulseHigh(FQ_UD);  // Done!  Should see output
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
  if(increment == 1){increment = 10; hertz = "10 Hz"; hertzPosition=5;} 
  else if(increment == 10){increment = 50; hertz = "50 Hz"; hertzPosition=5;}
  else if (increment == 50){increment = 100;  hertz = "100 Hz"; hertzPosition=4;}
  else if (increment == 100){increment = 500; hertz="500 Hz"; hertzPosition=4;}
  else if (increment == 500){increment = 1000; hertz="1 Khz"; hertzPosition=6;}
  else if (increment == 1000){increment = 2500; hertz="2.5 Khz"; hertzPosition=4;}
  else if (increment == 2500){increment = 5000; hertz="5 Khz"; hertzPosition=6;}
  else if (increment == 5000){increment = 10000; hertz="10 Khz"; hertzPosition=5;}
  else if (increment == 10000){increment = 100000; hertz="100 Khz"; hertzPosition=4;}
  else if (increment == 100000){increment = 1000000; hertz="1 Mhz"; hertzPosition=6;} 
  
  else{increment = 1; hertz = "1 Hz"; hertzPosition=5;};  
   lcd.setCursor(0,1);
     lcd.print("           ");
   lcd.setCursor(hertzPosition,1); 
   lcd.print(hertz);
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

//	Serial.println(rx); // debug vfo freq in serial console if need
	
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


//

////////////////////////////////
// translate cw code to ascii //
////////////////////////////////

void docode(){
    if (strcmp(code,".-") == 0) printascii(65);
	if (strcmp(code,"-...") == 0) printascii(66);
	if (strcmp(code,"-.-.") == 0) printascii(67);
	if (strcmp(code,"-..") == 0) printascii(68);
	if (strcmp(code,".") == 0) printascii(69);
	if (strcmp(code,"..-.") == 0) printascii(70);
	if (strcmp(code,"--.") == 0) printascii(71);
	if (strcmp(code,"....") == 0) printascii(72);
	if (strcmp(code,"..") == 0) printascii(73);
	if (strcmp(code,".---") == 0) printascii(74);
	if (strcmp(code,"-.-") == 0) printascii(75);
	if (strcmp(code,".-..") == 0) printascii(76);
	if (strcmp(code,"--") == 0) printascii(77);
	if (strcmp(code,"-.") == 0) printascii(78);
	if (strcmp(code,"---") == 0) printascii(79);
	if (strcmp(code,".--.") == 0) printascii(80);
	if (strcmp(code,"--.-") == 0) printascii(81);
	if (strcmp(code,".-.") == 0) printascii(82);
	if (strcmp(code,"...") == 0) printascii(83);
	if (strcmp(code,"-") == 0) printascii(84);
	if (strcmp(code,"..-") == 0) printascii(85);
	if (strcmp(code,"...-") == 0) printascii(86);
	if (strcmp(code,".--") == 0) printascii(87);
	if (strcmp(code,"-..-") == 0) printascii(88);
	if (strcmp(code,"-.--") == 0) printascii(89);
	if (strcmp(code,"--..") == 0) printascii(90);

	if (strcmp(code,".----") == 0) printascii(49);
	if (strcmp(code,"..---") == 0) printascii(50);
	if (strcmp(code,"...--") == 0) printascii(51);
	if (strcmp(code,"....-") == 0) printascii(52);
	if (strcmp(code,".....") == 0) printascii(53);
	if (strcmp(code,"-....") == 0) printascii(54);
	if (strcmp(code,"--...") == 0) printascii(55);
	if (strcmp(code,"---..") == 0) printascii(56);
	if (strcmp(code,"----.") == 0) printascii(57);
	if (strcmp(code,"-----") == 0) printascii(48);

	if (strcmp(code,"..--..") == 0) printascii(63);
	if (strcmp(code,".-.-.-") == 0) printascii(46);
	if (strcmp(code,"--..--") == 0) printascii(44);
	if (strcmp(code,"-.-.--") == 0) printascii(33);
	if (strcmp(code,".--.-.") == 0) printascii(64);
	if (strcmp(code,"---...") == 0) printascii(58);
	if (strcmp(code,"-....-") == 0) printascii(45);
	if (strcmp(code,"-..-.") == 0) printascii(47);

	if (strcmp(code,"-.--.") == 0) printascii(40);
	if (strcmp(code,"-.--.-") == 0) printascii(41);
	if (strcmp(code,".-...") == 0) printascii(95);
	if (strcmp(code,"...-..-") == 0) printascii(36);
	if (strcmp(code,"...-.-") == 0) printascii(62);
	if (strcmp(code,".-.-.") == 0) printascii(60);
	if (strcmp(code,"...-.") == 0) printascii(126);
	//////////////////
	// The specials //
	//////////////////
	if (strcmp(code,".-.-") == 0) printascii(3);
	if (strcmp(code,"---.") == 0) printascii(4);
	if (strcmp(code,".--.-") == 0) printascii(6);

}

/////////////////////////////////////
// print the ascii code to the lcd //
// one a time so we can generate   //
// special letters                 //
/////////////////////////////////////
void printascii(int asciinumber){

int fail = 0;
if (rows == 4 and colums == 16)fail = -4; /// to fix the library problem with 4*16 display http://forum.arduino.cc/index.php/topic,14604.0.html
 
 if (lcdindex > colums-1){
  lcdindex = 0;
  if (rows==4){
	  for (int i = 0; i <= colums-1 ; i++){
		lcd.setCursor(i,rows-3);
		lcd.write(line2[i]);
		line2[i]=line1[i];
	  }
   }
  for (int i = 0; i <= colums-1 ; i++){
    lcd.setCursor(i+fail,rows-2);
    lcd.write(line1[i]);
	lcd.setCursor(i+fail,rows-1);
    lcd.write(32);
  }
 }
 line1[lcdindex]=asciinumber;
 lcd.setCursor(lcdindex+fail,rows-1);
 lcd.write(asciinumber);
 lcdindex += 1;
}

void updateinfolinelcd(){
/////////////////////////////////////
// here we update the upper line   //
// with the speed.                 //
/////////////////////////////////////

  int place;
  if (rows == 4){
   place = colums/2;}
  else{
   place = 2;
  }
	if (wpm<10){
		lcd.setCursor((place)-2,0);
		lcd.print("0");
		lcd.setCursor((place)-1,0);
		lcd.print(wpm);
		lcd.setCursor((place),0);
		lcd.print(" WPM");
	}
	else{
		lcd.setCursor((place)-2,0);
		lcd.print(wpm);
		lcd.setCursor((place),0);
		lcd.print(" WPM ");
	}

}

void checkBTNdecode(){

// NEED TO STOP SOMETHING AND CHANGE TO USE BUTTON FOR CW DECODE (NOT FOR BAND CHANGE) !!!
  
      BTNdecodeON = digitalRead(BTNDEC);
      if(BTNdecodeON != BTNlaststate){
            if(BTNdecodeON == HIGH){
                  delay(220);
                  BTNcheck2 = 1;
                  BTNinc = BTNinc + 1;
                  switch (BTNinc) {
    case 1:
      rx=1800000;
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
      rx=21000000;
      break;
    case 8:
      rx=27000000;
      break;
    case 9:
      rx=28000000;
      break;    
    default:
      if(BTNinc > 9){
         BTNinc = 0;
      }
    break;
  }

//                  lcd.clear(); 	//CHECK IF NEED uncoment for cw decode
	          }
            if(BTNdecodeON == LOW){
                  BTNcheck2 = 0;
//                  lcd.clear();   //CHECK IF NEED uncoment for cw decode
	          }
            BTNlaststate = BTNcheck2;
            }
}

/*     UNCOMENT IF USE BUTTON FOR CW DECODE !!!
void checkBTNdecode(){
  
      BTNdecodeON = digitalRead(BTNDEC);
      if(BTNdecodeON != BTNlaststate){
            if(BTNdecodeON == HIGH){
                  BTNcheck = 1;
                  lcd.clear(); 
	          }
            if(BTNdecodeON == LOW){
                  BTNcheck = 0;
//                  lcd.clear(); 
	          }
            BTNlaststate = BTNcheck;
            }
}
           
*/

//// OK END
