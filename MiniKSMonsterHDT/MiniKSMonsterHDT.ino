/*******************************************

  Name.......:  MiniKSMonsterHDT (µ-Silver by HDT) 
  Description:  Arduino sketch for the kolloidal silver generator TBD is a fork of https://github.com/KSGeraeteSelbstbau/MiniKSMonsterEX, which forked
                https://github.com/SilberMonster-de/MiniKSMonster, which forked https://github.com/AgH2O/ks_shield
  Project....:  Is in early development phase and hosted on https://www.facebook.com/groups/kolloidalessilbergeraetetechnik/
  License....:  This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
                To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/ or send a letter to
                Creative Commons, 171 Second Street, Suite 300, San Francisco, California, 94105, USA.

********************************************/

#define VER "   v0.2"                         //Version 
#define OLED_DISPLAY                         //Comment out in case a 1602 LCD is used as display
// #define MINUTES                           //Uncomment if you want minutes instead of seconds for polarity change

#ifdef OLED_DISPLAY
  #include "SSD1306Ascii.h"                 // ascii library for Oled
  #include "SSD1306AsciiAvrI2c.h"
  #define I2C_ADDRESS 0x3C                  // 0X3C+SA0 - 0x3C or 0x3D
  SSD1306AsciiAvrI2c oled;                  // create short alias
#else
  //LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  #define I2C_ADDRESS 0x3F                 // 0x27 
  LiquidCrystal_I2C lcd(I2C_ADDRESS, 16, 2);
#endif

#include <EEPROM.h>                       // EEPROM library

#define SWL 6                             // switch def. left, middle, right
#define SWM 7
#define SWR 8

#define START 9                           // control line def.
#define AUDIO 5
#define POLW  10

#define TIMER_START TCCR1B |= (1 << CS12) | (0 << CS11) | (0 << CS10); // set bits
#define TIMER_STOP  TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10)); // deletes bits

float liter = 0.5;                       // set some start values
float ppm = 50;                           // wished ppm

boolean polaritaet = true;
boolean wassertest = true;                // water quality test default is disable
boolean display = true;                   // display enable

char text[32];

unsigned int taste, i, eine_minute, Position, adc_wert, adc_wert_a1;
unsigned int polwechselzeit1 = 10;
//unsigned int polwechselzeit2 = 10;
unsigned int polwechselzeit;
unsigned int bildwechselzeit = 10;
boolean bildwechsel = true;
//float polwechselschwelle = 2.5;
float spannung;
float strom_mess;
float strom_wassertest;                   // current measurement for water quality test
float schwellenwert_wassertest = 1.5;     // <1.5 water quality test passed, >1.5 water quality test failed
float shunt = 47;                         // 47 Ohm 0.1%

float Q_gesamt = 0;                       // new variables for ppm method
float Q_messung = 0;
float Q_remain = 0;
float faktor = 0.001118083;               // = M / z * F , 107,8782/1*96485
float mah = 0;
float schrittweite_adc = 2.490;
float zielmasse;
float masse;
unsigned int intervall = 1;               // measure every 1 sec.
long unsigned int sek = 0;
long unsigned int T_remain = 0;
char stringbuf[16];
unsigned int stunde; 
unsigned char minute, sekunde;
int b;

unsigned long previousMillis = 0;
int interval = 5000;                      // set the interval for the operation in milliseconds
unsigned long previousCounter = 0;
int intervaldisplay = 60000;              // set the interval for display protection in seconds

int filterfactor;
float filter;

void setup() {
  
                                          // Interrupt Routine Setup Timer1 1 Hz
                                          // TIMER 1 for interrupt frequency 1 Hz:
  cli();                                  // stop interrupts
  TCCR1A = 0;                             // set entire TCCR1A register to 0
  TCCR1B = 0;                             // same for TCCR1B
  TCNT1  = 0;                             // initialize counter value to 0
                                          // set compare match register for 1 Hz increments
  OCR1A = 62499;                          // = 16000000 / (256 * 1) - 1 (must be <65536)
  TCCR1B |= (1 << WGM12);                 // turn on CTC mode
                                          // Set CS12, CS11 and CS10 bits for 256 prescaler
  TCCR1B |= (1 << CS12) | (0 << CS11) | (0 << CS10);
  TIMSK1 |= (1 << OCIE1A);                // enable timer compare interrupt
  // TIMSK1 |= (0 << OCIE1A);             // disable timer compare interrupt 
  sei();                                  // allow interrupts
  TIMER_STOP

  pinMode(SWL, INPUT);                    // setup Switches + PullDown
  digitalWrite(SWL, LOW);
  pinMode(SWM, INPUT);
  digitalWrite(SWM, LOW);
  pinMode(SWR, INPUT);
  digitalWrite(SWR, LOW);

  pinMode(START, OUTPUT);                 // setup control lines
  digitalWrite(START, HIGH);              // start value high == stop
  pinMode(AUDIO, OUTPUT);
  pinMode(POLW, OUTPUT);
  digitalWrite(POLW, LOW);                // start value low

  #ifdef OLED_DISPLAY
    oled.begin(&Adafruit128x64, I2C_ADDRESS);// setup Oled
    oled.setContrast(100);                  // contrast adjustment
    oled.clear();
    oled.set2X();
    oled.setFont(X11fixed7x14B);
    oled.println(" mySilver");               // start message
    oled.println(" by HDT ");
    delay(2000);
    oled.clear();
    oled.setCursor (0,2); 
    oled.println(VER);
    oled.set1X();
    oled.setFont(fixed_bold10x15);
    delay(2000);                            // 2 sec. are enough
  #else
    lcd.init();                            // setup LCD
    lcd.backlight();                        // backlight on
    lcd.clear();
    lcd.print(" mySilver");                // start message
    lcd.setCursor (0,1);
    lcd.print(" by HDT ");
    lcd.print(VER);
    delay(2000);                            // 2 sec. are enough
  #endif
  
  analogReference(EXTERNAL);              // analog Reference external 2.5V

  int eeAddress = 1;                      // read EEPROM
  int eepromtest = 0;
  eepromtest = EEPROM.read(0);
  if (eepromtest == 1) {
    liter = EEPROM.get(eeAddress, liter);
    eeAddress += sizeof(float); //Move address to the next byte after float 'liter'.
    ppm = EEPROM.get(eeAddress, ppm);
    eeAddress += sizeof(float); //Move address to the next byte after float 'ppm'.
    polwechselzeit1 = EEPROM.get(eeAddress, polwechselzeit1);
//    eeAddress += sizeof(unsigned int); //Move address to the next byte after unsigned int 'polwechselzeit1'.
//    polwechselzeit2 = EEPROM.get(eeAddress, polwechselzeit2);
//    eeAddress += sizeof(unsigned int); //Move address to the next byte after unsigned int 'polwechselzeit2'.
//    polwechselschwelle = EEPROM.get(eeAddress, polwechselschwelle);
  }
}

uint8_t lese_tasten(void) {               // function reads switches and returns 1-7
  uint8_t zwi_speich = 0;
  if (digitalRead(SWL) == HIGH)
    zwi_speich = 1;
  if (digitalRead(SWM) == HIGH)
    zwi_speich += 2;
  if (digitalRead(SWR) == HIGH)
    zwi_speich += 4;
  return zwi_speich;

}

void print_wassermenge(float liter) {     // 1. display output - amount of water
  #ifdef OLED_DISPLAY
    oled.clear();
    oled.setCursor(3, 0);
    oled.print("Wassermenge");
    oled.setCursor(8, 4);
    oled.print(liter);
    oled.print(" Liter");
  #else
    //LCD
    lcd.clear();
    lcd.print("Wassermenge");
    lcd.setCursor(0, 1);
    lcd.print(liter);
    lcd.print(" Liter");
  #endif
}

void print_ppm(float ppm) {               // 2. display output - wished ppm
  #ifdef OLED_DISPLAY
    oled.clear();
    oled.setCursor(9, 0);
    oled.print("KS Staerke");
    oled.setCursor(29, 4);
    oled.print((int)ppm);
    oled.print(" ppm ");
  #else
    //LCD
    lcd.clear();
    lcd.print("KS Staerke");
    lcd.setCursor(0, 1);
    lcd.print((int)ppm);
    lcd.print(" ppm ");
  #endif
}

void print_polw1(unsigned int polwechselzeit1) {
  #ifdef OLED_DISPLAY
    oled.clear();                           // 3. display output - change polarioty time
    oled.setCursor(4, 0);
    oled.print("Umpolzeit");
    oled.setCursor(26, 4);
    oled.print(polwechselzeit1);
    #ifdef MINUTES
      oled.print(" Min.");
    #else
      oled.print(" Sek.");
    #endif
  #else
    //LCD
    lcd.clear();                           // 3. display output - change polarioty time
    lcd.print("Umpolzeit");
    lcd.setCursor(0, 1);
    lcd.print(polwechselzeit1);
    #ifdef MINUTES
      lcd.print(" Min.");
    #else
      lcd.print(" Sek.");
    #endif
  #endif
}

//void print_schwelle(float polwechselschwelle) {
//  #ifdef OLED_DISPLAY
//    oled.clear();                           // 4. display output - change polarioty time
//    oled.setCursor(4, 0);
//    oled.print(" Schwelle");
//    oled.setCursor(26, 4);
//    oled.print(polwechselschwelle);
//    oled.print(" mA");
//  #else
//    //LCD
//    lcd.clear();                           // 4. display output - change polarioty time
//    lcd.print("Schwelle");
//    lcd.setCursor(0, 1);
//    lcd.print(polwechselschwelle);
//    lcd.print(" mA");
//   #endif
//}

//void print_polw2(unsigned int polwechselzeit2) {
//  #ifdef OLED_DISPLAY
//    oled.clear();                           // 5. display output - change polarioty time
//    oled.setCursor(4, 0);
//    oled.print("Umpolzeit 2");
//    oled.setCursor(26, 4);
//    oled.print(polwechselzeit2);
//    #ifdef MINUTES
//      oled.print(" Min.");
//    #else
//      oled.print(" Sek.");
//    #endif
//  #else
//    //LCD
//    lcd.clear();                           // 5. display output - change polarioty time
//    lcd.print("Umpolzeit 2");
//    lcd.setCursor(0, 1);
//    lcd.print(polwechselzeit2);
//    #ifdef MINUTES
//      lcd.print(" Min.");
//    #else
//      lcd.print(" Sek.");
//    #endif
//  #endif
//}

void print_wassertest(void) {             // 6. display output - water quality test and Start question
  #ifdef OLED_DISPLAY
    oled.clear();
    if (wassertest) {
      oled.setCursor(0, 0);
      oled.print("Wassertest: ");
      digitalWrite(START, LOW);
      delay(200);
      adc_wert_a1 = 0;
      for (int i = 1; i <= 10; i++) {
        delay(20);
        adc_wert_a1 = adc_wert_a1 + analogRead(1);
      }
      adc_wert_a1 = adc_wert_a1 / 10;
      strom_wassertest = (float) (adc_wert_a1 * schrittweite_adc);
      strom_wassertest = (strom_wassertest / shunt);
      delay(200);
      oled.setCursor(0, 2);
      if (strom_wassertest < schwellenwert_wassertest) {
       oled.print("OK ");
       oled.print(strom_wassertest);
       oled.print(" mA");
      } else {
       oled.print("NOK ");
       oled.print(strom_wassertest);
       oled.print(" mA");
      }
      delay(200);
      digitalWrite(START, HIGH);
      oled.setCursor(0, 4);
      oled.set2X();
      oled.print("Start?");
      oled.set1X();
    } else {
      oled.setCursor(0, 2);
      oled.set2X();
      oled.print("Start?");
      oled.set1X();
    }
  #else
    lcd.clear();
    if (wassertest) {
      lcd.setCursor(0, 0);
      lcd.print("Wassertest: ");
      digitalWrite(START, LOW);
      delay(200);
      adc_wert_a1 = 0;
      for (int i = 1; i <= 10; i++) {
        delay(20);
        adc_wert_a1 = adc_wert_a1 + analogRead(1);
      }
      adc_wert_a1 = adc_wert_a1 / 10;
      strom_wassertest = (float) (adc_wert_a1 * schrittweite_adc);
      strom_wassertest = (strom_wassertest / shunt);
      delay(200);
      if (strom_wassertest < schwellenwert_wassertest) {
        lcd.print("OK");
      } else {
        lcd.print("NOK");
      }
      delay(200);
      digitalWrite(START, HIGH);
      lcd.setCursor(0, 1);
      lcd.print("Start?");
      } else {
      lcd.print("Start?");
      }
    #endif
}

void erste_zeile_clean() {                // Clean first line
  #ifdef OLED_DISPLAY
    oled.home();
    oled.print("                 ");
  #else
    lcd.home();
    lcd.print("                 ");
  #endif
}

void zweiSekunden(void) {                 // 2 sec. delay but reads left and right switch
  for (i = 0; i < 100; i ++)
    if ((lese_tasten() == 4) || (lese_tasten() == 1) )
      break;
    else
      delay(20);
}

void software_Reset() {                   // Restarts program from beginning but
  //  asm volatile ("  jmp 0");           // does not reset the peripherals and registers
}
                                          // function calculates target mass
float errechne_zielmasse(float ppm, float wasser) {
  return (wasser * 1000 / 1000000) * ppm; // mass = (water * 1000 (gramm)/ 1000000) * ppm
}
                                          // function calculates ppm from mass
float masse2ppm(float masse, float liter) {
  return (masse / (liter * 1000 / 1000000));
}

void sek2hhmmss(long int zeit) {          // format clock/counter
  if (zeit > 59) {
    minute++;
    sek = 0;
  }
  if (minute > 59) {
    stunde++;
    minute = 0;
  }
  if (stunde > 9) {                       // because 1 digit 9 max.
    stunde = 0;
  }
}

void secondsToHMS( const uint32_t seconds, uint16_t &h, uint8_t &m, uint8_t &s )
{
    uint32_t t = seconds;

    s = t % 60;

    t = (t - s)/60;
    m = t % 60;

    t = (t - m)/60;
    h = t;
}

ISR(TIMER1_COMPA_vect) {                  // Interrupt Routine every 1 sec

  adc_wert = analogRead(0);               // measure U on voltage divider
  adc_wert_a1 = analogRead(1);            // measure I on shunt 47 ohm

  filterfactor = 10;                      // filter factor
  filter = ((filter * filterfactor) + adc_wert_a1) / (filterfactor + 1);

  spannung = (float) adc_wert * 0.00244;  // 0.00244140625 (2,5V /1024) accurate value important
                                          // Ushunt * 1000 = mV
  strom_mess = (float) (filter * schrittweite_adc);
  strom_mess = (strom_mess / shunt) ;     // I = Ushunt / Rshunt

  Q_messung =  strom_mess  * intervall;   // Coulomb = I * t
  Q_gesamt = Q_gesamt + Q_messung;        // Q added
  masse = (Q_gesamt / 1000) * faktor;     // Qgesamt from mC to C
  Q_remain = ((zielmasse - masse) / faktor) * 1000;    // Q remain
  
  if (strom_mess != 0) {                     //prevent devision by zero if measured current gets zero 
    T_remain = int (Q_remain / strom_mess);  //remaining time in sec
  }
       
  mah = (Q_gesamt / 1000) * 0.2795476873690739;  

  ppm = masse2ppm(masse, liter);

  #ifdef MINUTES
    polwechselzeit = polwechselzeit1*60;
  #else
    polwechselzeit = polwechselzeit1;
  #endif

//  if (strom_mess < polwechselschwelle)
//    {
//      #ifdef MINUTES
//        polwechselzeit = polwechselzeit1*60;
//      #else
//        polwechselzeit = polwechselzeit1;
//      #endif
//    }
//    else
//    {
//      #ifdef MINUTES
//        polwechselzeit = polwechselzeit2*60;
//      #else
//        polwechselzeit = polwechselzeit2;
//      #endif
//    }

  if (!(i % (bildwechselzeit)))            // Screenchange to V
    { 
      if (i != 0) {
        bildwechsel = !bildwechsel;
        } 
    }

  if (polwechselzeit != 0) 
    {    
    if (!(i % (polwechselzeit)))             // Polarity change every 15 sec./ basis time
      { polaritaet = !polaritaet;
        digitalWrite(START, HIGH);
        delay(500);
        digitalWrite(POLW, polaritaet);
        delay(500);
        digitalWrite(START, LOW);
      }
    }
    
  sek++;
  i++;                                     // intervall x i = total time
}

void print_loop(boolean screen) {
  if (display) {
     #ifdef OLED_DISPLAY
       oled.setCursor(18, 0);                 // Oled 1. row
       if (screen) {
          secondsToHMS(sek, stunde, minute, sekunde);               
          sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
          oled.print(stringbuf);
        } else {
          if (strom_mess > 1) { 
            secondsToHMS(T_remain, stunde, minute, sekunde);              
            sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
            oled.print(stringbuf);
          }else{
            oled.print("-:--:--");             // if measured current is below 1 mA do not display remaining time
          }
        } 
       oled.setCursor(18, 2);                 // Oled 2. row
       if (screen) {
          oled.print(ppm);
          oled.print(" ppm    ");
        } else {
          oled.print(masse2ppm(zielmasse-masse, liter));
          oled.print(" ppm    ");
        } 
       oled.setCursor(18, 4);                 // Oled 3. row
        if (screen) {
          oled.print(mah);
          oled.print(" mAh       ");
          oled.setCursor(18, 6);                 // Oled 4. row
          oled.print(strom_mess);
          oled.print(" mA   ");
       } else {
          oled.print((int)(spannung * 67.76)); // factor measured voltage divider
          oled.print(" Volt      ");
          oled.setCursor(18, 6);                 // Oled 4. row
          oled.print(strom_mess);
          oled.print(" mA R");
       }
        
      } else {
      oled.clear();
      }
      #else
        lcd.setCursor(1, 0);
        if (screen) {
          lcd.print("T ");
          secondsToHMS(sek, stunde, minute, sekunde);               
          sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
          lcd.print(stringbuf);
        } else {
          lcd.print("R ");
          if (strom_mess > 1) {
            secondsToHMS(T_remain, stunde, minute, sekunde);               
            sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
            lcd.print(stringbuf);
          }else{
            lcd.print("-:--:--");             // if measured current is below 1 mA do not display remaining time
        }  
        lcd.setCursor(11, 0);
        if (screen) {
          lcd.print("P ");
          sprintf(stringbuf, "%03d", (int)ppm);           
          lcd.print(stringbuf);
        } else {
          lcd.print("P ");
          sprintf(stringbuf, "%03d", (int)masse2ppm(zielmasse-masse, liter));           
          lcd.print(stringbuf);
        }  
        lcd.setCursor(0, 1);                 
        if (screen) {
          lcd.print("mAh");
          sprintf(stringbuf, "%03d.%01d", (int)mah, (int)(mah*10)%10);
          lcd.print(stringbuf);
        } else {
          lcd.print("V ");
          lcd.print((int)(spannung * 67.76)); // factor measured voltage divider
        }
        lcd.setCursor(9, 1);
        lcd.print("mA");
        sprintf(stringbuf, "%02d.%02d", (int)strom_mess, (int)(strom_mess*100)%100);
        lcd.print(stringbuf);
        } else {
        lcd.clear();
      }
      #endif
    
    unsigned long currentCounter = i;        // automatic display shutdown
    if ((unsigned long)(currentCounter - previousCounter) >= intervaldisplay) {
      display = false;
      if (lese_tasten() >= 1) {
        previousCounter = currentCounter;
      }
    } else {
      display = true;
    }
    if (display) {
        if (polaritaet) {
          #ifdef OLED_DISPLAY
            oled.setCursor(105, 0); oled.print("-");
          #else
            lcd.setCursor(0, 0); lcd.print("-");
          #endif
         } else {
          #ifdef OLED_DISPLAY
            oled.setCursor(105, 0); oled.print("+");
          #else
            lcd.setCursor(0, 0); lcd.print("+");
          #endif
        }
     }
}

void biep(void) {
  tone(AUDIO, 2600);                       // beep
  delay(50);
  noTone(AUDIO);
}

void biep2(void) {
  tone(AUDIO, 2600);                       // beep2
  delay(1000);
  noTone(AUDIO);
  delay(400);
}

// *** M A I N L O O P ***
void loop() {
  do {                                     // back to loop
    print_wassermenge(liter);
    do {                                   // choose amout of water
      if (lese_tasten() == 1) {
        if (liter < 5.00)
          if (liter <= 0.79) {
            liter += 0.1 ;
          } else {
            liter += 0.1 ;
          }
        #ifdef OLED_DISPLAY  
          oled.setCursor(8, 4);
          oled.print(liter);
        #else
          lcd.setCursor(0, 1);
          lcd.print(liter);
        #endif
        biep();
      }
      if (lese_tasten() == 2) {
        if (liter > 0.05)
          if (liter < 0.80) {
            liter -= 0.1 ;
          } else {
            liter -= 0.1 ;
          }
        #ifdef OLED_DISPLAY  
          oled.setCursor(8, 4);
          oled.print(liter);
        #else
          lcd.setCursor(0, 1);
          lcd.print(liter);
        #endif
        biep();
      }
      unsigned long currentMillis = millis();
      if ((unsigned long)(currentMillis - previousMillis) >= interval) {
        delay(50);
        if (lese_tasten() == 0) {
          previousMillis = currentMillis;
        }
      } else {
        delay(300);
      }
    } while (lese_tasten() != 4);
    biep();

    print_ppm(ppm);
    do {
      if (lese_tasten() == 1) {
        if (ppm < 1000)
          if (ppm < 20) {
            ppm += 5 ;
          } else {
            ppm += 5 ;
          }
        #ifdef OLED_DISPLAY 
          oled.setCursor(29, 4);
          oled.print((int) ppm);
          oled.println(" ppm  ");
        #else
          lcd.setCursor(0, 1);
          lcd.print((int) ppm);
          lcd.print(" ppm  ");
        #endif
        biep();
      }
      if (lese_tasten() == 2) {
        if (ppm > 1)
          if (ppm < 20) {
            ppm -= 5 ;
          } else {
            ppm -= 5 ;
          }
        #ifdef OLED_DISPLAY 
          oled.setCursor(29, 4);
          oled.print((int) ppm);
          oled.println(" ppm  ");
        #else
          lcd.setCursor(0, 1);
          lcd.print((int) ppm);
          lcd.print(" ppm  ");
        #endif
        biep();
      }
      unsigned long currentMillis = millis();
      if ((unsigned long)(currentMillis - previousMillis) >= interval) {
        delay(50);
        if (lese_tasten() == 0) {
          previousMillis = currentMillis;
        }
      } else {
        delay(300);
      }
    } while (lese_tasten() != 4);
    biep();

print_polw1(polwechselzeit1);
    do {                                   // choose polarity change time
      if (lese_tasten() == 1) {
        if (polwechselzeit1 < 600)         // top max. 600 sec = 10 Min
          polwechselzeit1 += 10 ;          // polarity change +10s
        #ifdef OLED_DISPLAY
          oled.setCursor(26, 4);
          oled.print(polwechselzeit1);
          #ifdef MINUTES
            oled.print(" Min. ");
          #else
            oled.print(" Sek. ");
          #endif
          #else
            lcd.setCursor(0, 1);
            lcd.print(polwechselzeit1);
          #ifdef MINUTES
            lcd.print(" Min. ");
          #else
            lcd.print(" Sek. ");
          #endif
        #endif
        biep();
      }
      if (lese_tasten() == 2) {
        if (polwechselzeit1)               // bottom min.
          polwechselzeit1 -= 10 ;           // polarity change -10s
        #ifdef OLED_DISPLAY
          oled.setCursor(26, 4);
          oled.print(polwechselzeit1);
          #ifdef MINUTES
            oled.print(" Min. ");
          #else
            oled.print(" Sek. ");
          #endif
          #else
            lcd.setCursor(0, 1);
            lcd.print(polwechselzeit1);
          #ifdef MINUTES
            lcd.print(" Min. ");
          #else
            lcd.print(" Sek. ");
          #endif
        #endif
        biep();
      }
      unsigned long currentMillis = millis();
      if ((unsigned long)(currentMillis - previousMillis) >= interval) {
        delay(50);
        if (lese_tasten() == 0) {
          previousMillis = currentMillis;
        }
      } else {
        delay(300);
      }
    } while (lese_tasten() != 4);
    biep();
    
//    print_schwelle(polwechselschwelle);
//    do {                                   // choose polarity change time
//      if (lese_tasten() == 1) {
//        if (polwechselschwelle < 10)       // top max. 10mA
//          polwechselschwelle += 0.1 ;
//        #ifdef OLED_DISPLAY
//          oled.setCursor(26, 4);
//          oled.print(polwechselschwelle);
//          oled.print(" mA ");
//        #else
//          lcd.setCursor(0, 1);
//          lcd.print(polwechselschwelle);
//          lcd.print(" mA ");
//        #endif  
//        biep();
//      }
//      if (lese_tasten() == 2) {
//        if (polwechselschwelle)            // bottom min.
//          polwechselschwelle -= 0.1 ;
//        #ifdef OLED_DISPLAY
//          oled.setCursor(26, 4);
//          oled.print(polwechselschwelle);
//          oled.print(" mA ");
//        #else
//          lcd.setCursor(0, 1);
//          lcd.print(polwechselschwelle);
//          lcd.print(" mA ");
//        #endif  
//        biep();
//      }
//      unsigned long currentMillis = millis();
//      if ((unsigned long)(currentMillis - previousMillis) >= interval) {
//        delay(50);
//        if (lese_tasten() == 0) {
//          previousMillis = currentMillis;
//        }
//      } else {
//        delay(300);
//      }
//    } while (lese_tasten() != 4);
//    biep();
    
//    print_polw2(polwechselzeit2);
//    do {                                  // choose polarity change time
//      if (lese_tasten() == 1) {
//        if (polwechselzeit2 < 600)        // top max. 600 sec = 10 Min
//          polwechselzeit2 += 10 ;         // polarity change +10s
//        #ifdef OLED_DISPLAY
//          oled.setCursor(26, 4);
//          oled.print(polwechselzeit2);
//          #ifdef MINUTES
//            oled.print(" Min. ");
//          #else
//            oled.print(" Sek. ");
//          #endif
//          #else
//            lcd.setCursor(0, 1);
//            lcd.print(polwechselzeit2);
//          #ifdef MINUTES
//            lcd.print(" Min. ");
//          #else
//            lcd.print(" Sek. ");
//          #endif
//        #endif
//        biep();
//      }
//      if (lese_tasten() == 2) {
//        if (polwechselzeit2)              // bottom min.
//          polwechselzeit2 -= 10 ;         // polarity change -10s
//        #ifdef OLED_DISPLAY
//          oled.setCursor(26, 4);
//          oled.print(polwechselzeit2);
//          #ifdef MINUTES
//            oled.print(" Min. ");
//          #else
//            oled.print(" Sek. ");
//          #endif
//          #else
//            lcd.setCursor(0, 1);
//            lcd.print(polwechselzeit2);
//          #ifdef MINUTES
//            lcd.print(" Min. ");
//          #else
//            lcd.print(" Sek. ");
//          #endif
//        #endif 
//        biep();
//      }
//      unsigned long currentMillis = millis();
//      if ((unsigned long)(currentMillis - previousMillis) >= interval) {
//        delay(50);
//        if (lese_tasten() == 0) {
//          previousMillis = currentMillis;
//        }
//      } else {
//        delay(300);
//      }
//    } while (lese_tasten() != 4);
//    biep();
//    
    int eeAddress = 1;                    // write EEPROM
    EEPROM.write(0, 1);
    EEPROM.put(eeAddress, liter);
    eeAddress += sizeof(float); //Move address to the next byte after float 'liter'.
    EEPROM.put(eeAddress, ppm);
    eeAddress += sizeof(float); //Move address to the next byte after float 'ppm'.
    EEPROM.put(eeAddress, polwechselzeit1);
//    eeAddress += sizeof(unsigned int); //Move address to the next byte after float 'polwechselzeit1'.
//    EEPROM.put(eeAddress, polwechselzeit2);
//    eeAddress += sizeof(unsigned int); //Move address to the next byte after float 'polwechselzeit2'.
//    EEPROM.put(eeAddress, polwechselschwelle);

    #ifdef OLED_DISPLAY
      oled.clear();
    #else
      lcd.clear();
    #endif
    delay(1000);
    do {                                  // last user check
      #ifdef OLED_DISPLAY
        oled.setCursor(0, 0);
        oled.print((int)ppm);
        oled.print(" ppm");
        oled.setCursor(0, 2);
        oled.print(liter);
        oled.print(" Liter");
        oled.setCursor(0, 4);
        oled.print((int)ppm/5);
        oled.print(" mg/L");
        oled.setCursor(0, 6);
        oled.print("Wasser kalt");
      #else
        lcd.setCursor(0, 0);
        lcd.print("P ");
        lcd.print((int)ppm);
        lcd.setCursor(8, 0);
        lcd.print("l ");
        lcd.print(liter);
        lcd.setCursor(0, 1);
        lcd.print("mgl ");
        lcd.print((int)ppm/5);
        lcd.setCursor(0, 1);
        lcd.print("W kalt");
      #endif  
      taste = lese_tasten();
      zweiSekunden();                     // delay against flickering display but read keys
    } while (taste != 4 && taste != 2 && taste != 1); //action if key hit, leave while loop
    biep();
  } while ( taste != 4 && taste != 2);    // only **not** back, if 4 == go further

  delay(1000);
  print_wassertest();                     // water quality test and Start question
  do {
    taste = lese_tasten();                // go futher, on every key
  } while (taste != 4);
  biep();

  switch (taste)
  {
    case 1:
    case 2:
    case 4:
      #ifdef OLED_DISPLAY
        oled.clear();
      #else
        lcd.clear();
      #endif
      zielmasse = errechne_zielmasse(ppm, liter);
      digitalWrite(START, LOW);           // Start DC/DC Converter
      TIMER_START
      do {                                // ACTIONLOOP UNTIL KS FINISHED
        delay(100);                       // 0,1 sec.
        print_loop(bildwechsel);                     // 
        if (lese_tasten() == 7) {         // shall i do  a soft reset?
          biep();
          software_Reset();
        }
      } while (masse <= zielmasse);       // ACTIONLOOP END

      digitalWrite(START, HIGH);          // Stop DC/DC Converter
      TIMER_STOP
      digitalWrite(POLW, LOW);
      #ifdef OLED_DISPLAY
        oled.clear();
        oled.setCursor(18, 0);              // Oled 1. row
        secondsToHMS(sek, stunde, minute, sekunde);
        sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
        oled.print(stringbuf);
        oled.setCursor(18, 2);              // Oled 2. row
        oled.print(ppm);
        oled.print(" ppm    ");
        oled.setCursor(18, 4);              // Oled 3. row
        oled.print(mah);
        oled.print(" mAh       ");
        oled.setCursor(18, 6);              // Oled 4. row
        oled.print("KS fertig");
      #else
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("T ");
        secondsToHMS(sek, stunde, minute, sekunde);
        sprintf(stringbuf, "%01d:%02d:%02d", stunde, minute, sekunde);
        lcd.print(stringbuf);
        lcd.setCursor(12, 0);
        lcd.print("P ");              
        lcd.print(ppm);
        lcd.setCursor(0, 1);
        lcd.print("Ah ");
        sprintf(stringbuf, "%03d.%01d", (int)mah, (int)(mah*10)%10);
        lcd.print(stringbuf);
        lcd.setCursor(9, 1);              
        lcd.print("Fertig!");
      #endif
      for (b = 1; b <= 10; b++ ) {        // Finished and beep 10 times
        biep2();
      }
      break;
  }

  do {                                    // Wait loop until any key is pressed
    taste = lese_tasten();
  } while (taste != 4 && taste != 2 && taste != 1);
  Q_gesamt = 0; i = 0; sek = 0; stunde = 0; minute = 0; zielmasse = 0; masse = 0; display = true; // Reset counter
  delay(1000);
}
