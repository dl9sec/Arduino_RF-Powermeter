/*
  RF-Powermeter_1v0.ino

  RF-Powermeter with for a Mini-Circuits ZX47-55-S+ frontend, 128x64 OLED display and pushbuttons.
  Using Arduino Uno (5V/16MHz) and Waveshare 0.96" 128x64 OLED SPI/I2C with SSD1306.
  
  Copyright (C) 2019 by Thorsten Godau (dl9sec) and licensed under
  GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

  Connections
  ===========
  
  Arduino             Signal
  -------             ------

  (4) --------------- Upper  pushbutton to GND
  (3) --------------- Middle pushbutton to GND
  (2) --------------- Lower  pushbutton to GND
  
  (A0) -------------- ZX47 TEMP
  (A1) -------------- ZX47 DC OUT
  (5V) -------------- ZX47 +5V
  (GND) ------------- ZX47 GND
  
  (5V) -------------- OLED VCC
  (GND) ------------- OLED GND
  (11) -------------- OLED DIN
  (13) -------------- OLED CLK
  (10) -------------- OLED CS
  (9) --------------- OLED D/C
  (8) --------------- OLED RES
  
  (AREF) --- 10k ---- 3V3
  
  Usage
  =====
  
  The ZX47 linear measurement range at about 2,4GHz is between -55dBm and +5dBm.
  For more than +5dBm input power, an suitable attenuator must be connected ahead.
  The ATT value is just added to the measured power value.
  The value of the ATT can be adjusted with upper/middle pushbutton:
  
  - Press upper button  -> Increase ATT
  - Press middle button -> Decrease ATT
  
  The ATT range is 0..49dB, so RF power up to +54dBm/251W can be displayed.
  
  If the ATT value was changed, the new value is stored at the internal EEPROM address
  0 after 200ms.
  
  Pressing all three pushbuttons at the same time will reset the device
  and displays the splash screen.  
  
  Remove comment at "#define DEBUGON" to get debug information as measured voltages and
  pushbutton states.
*/

#include <EEPROM.h>
#include <U8g2lib.h>

// Prototypes
void (*softReset)(void) = 0;  // JMP 0

// Debug on/off
//#define DEBUGON

// Version string
#define VERSION         F("v1.0")

// Digital pins
#define DI_PBU          4   // Upper pushbutton
#define DI_PBM          3   // Middle pushbutton
#define DI_PBL          2   // Lower pushbutton

// OLED pin definitions
#define OLED_RES        8
#define OLED_DC         9
#define OLED_CS         10

// Power and temperature measurement
#define AI_RFPWR        A1  // Analog input for ZX47 RF power equivalent voltage
#define AI_AMBTEMP      A0  // Analog input for ambient temperature equivalent voltage


// The value of the ADC reference voltage
// AREF connected via 10k to 3V3. With internal 32k -> ADCREF = 3,3V * 32k / (32k + 10k)
// Calcualted: 2.514V, Measured: 2.542V
#define ADCREF          2.542


// HW-SPI: CLK = 13, DIN = 11, CS = 10, DC = 9, RES = 8 / 180°
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R2, OLED_CS, OLED_DC, OLED_RES);


// Globals
char     strTempAmb[8]  = "       ";        // Ambient temperature string
//                        "<-55 °C"
//                        ">100 °C"
char     strPwrdBm[10]  = "         ";      // dBm power string
//                        "-50.0 dBm"
//                        " 54.0 dBm"
char     strPwrW[9]     = "        ";       // Watt power string
//                        "999.9 W "
//                        "999.9 mW"
//                        "999.9 uW"
//                        "999.9 nW"
char     strAtt[6]      = "     ";          // Attenuation string
//                        " 0 dB"
//                        "49 dB"

char     strUA0[5]      = "    ";           // A0 voltage of temperature (debug)
char     strUA1[5]      = "    ";           // A1 voltage of RF power (debug)
//                        "1.88"

char     strPBstate[5]  = "    ";           // Pusbutton state (debug)
//                        "PB:7"

uint8_t  u8Att          = 0;                // Attenuation
uint8_t  u8AttLast      = 0;                // Last attenuation
uint8_t  u8Count        = 0;                // Counter variable
uint8_t  u8IdxUPwrRaw   = 0;                // Index for battery averaging array  
uint8_t  u8IdxUTempRaw  = 0;                // Index for battery averaging array  
int16_t  as16UPwrRaw[20];                   // Initialized in setup()
int16_t  as16UTempRaw[20];                  // Initialized in setup()

double   dPwrW          = 0.0;              // Calculated RF power W
float    fPwrdBm        = 0.0;              // Calculated RF power dBm
float    fTamb          = 0.0;              // Calculated ambient temperature

int16_t  s16Tmp         = 0;                // Calculation dummy
char     strTmp[10]     = "         ";      // Temporary formatting dummy

int16_t  s16pushButtons = 0;                // State of the pushbuttons

uint32_t u32MillisCur   = 0;                // Current milliseconds
uint32_t u32MillisLast  = 0;                // Last milliseconds


void setup(void)
{
  // Set analog external reference
  analogReference(EXTERNAL);
  
  // Init digital I/Os
  pinMode(DI_PBU, INPUT_PULLUP);
  pinMode(DI_PBM, INPUT_PULLUP);
  pinMode(DI_PBL, INPUT_PULLUP);

  // Init ATT from EEPROM
  if ( EEPROM[0] > 49 )
  {
    EEPROM[0] = 0;  // ATT value not valid, set to 0
  }
  else
  {
    u8Att     = EEPROM[0];  // Read ATT value from EEPROM address 0
    u8AttLast = u8Att;
  }

  // Init power measurement array
  for ( u8Count = 0; u8Count < (sizeof(as16UPwrRaw)/2); u8Count++ )
  {
    as16UPwrRaw[u8Count] = analogRead(AI_RFPWR);      // Read ADC and fill array initially
  }

  // Init temperature measurement array
  for ( u8Count = 0; u8Count < (sizeof(as16UTempRaw)/2); u8Count++ )
  {
    as16UTempRaw[u8Count] = analogRead(AI_AMBTEMP);   // Read ADC and fill array initially
  }

  u32MillisLast = millis();

  // Init display
  u8g2.begin();

  u8g2.clearBuffer(); // Clear the internal display memory
  u8g2.setFont(u8g2_font_10x20_tf);
  showSplash();
  u8g2.sendBuffer();  // Transfer internal memory to the display
  delay(2000);

}


void loop(void)
{
  // ===== Attenuation =====
  // Read pushbutton states
  s16pushButtons  =  (int)(!digitalRead(DI_PBU));
  s16pushButtons |= ((int)(!digitalRead(DI_PBM))) << 1;
  s16pushButtons |= ((int)(!digitalRead(DI_PBL))) << 2;

#if defined(DEBUGON)
  sprintf(strPBstate,"PB:%d", s16pushButtons);
#endif  

  u32MillisCur = millis();
  
  if ( (u32MillisCur - u32MillisLast) > 200 )
  {
    if ( s16pushButtons )
    {
      switch(s16pushButtons)
      {
        case 1:   // Upper PB -> Attenuation++
                  if ( ++u8Att > 49 )
                  {
                    u8Att = 49;
                  }
                  break;

        case 2:   // Middle PB -> Attenuation--
                  if ( u8Att > 0 )
                  {
                    u8Att--;
                  }
                  break;

        case 7:   // Upper PB + Middle PB + Lower PB -> Reset
                  softReset();
                  break;
        
        default:  break;
        
      }

    if ( u8Att != u8AttLast )
    {
      EEPROM[0] = u8Att;
      u8AttLast = u8Att;
    }

    u32MillisLast  = millis();
    }
  }

  // ===== RF power =====
  // Read RF power equivalent voltage and filter
  as16UPwrRaw[u8IdxUPwrRaw] = analogRead(AI_RFPWR);  // Read RF power equivalent voltage (raw ADC)
  
  if ( ++u8IdxUPwrRaw > ( (sizeof(as16UPwrRaw) / 2) - 1 ) )
  {
    u8IdxUPwrRaw = 0;  // Reset index
  }

   // Accumulate array and average
  s16Tmp = 0;
  for ( u8Count = 0; u8Count < (sizeof(as16UPwrRaw) / 2); u8Count++ )
  {
    s16Tmp += as16UPwrRaw[u8Count];
  }
  
  s16Tmp = s16Tmp / (sizeof(as16UPwrRaw) / 2);

#if defined(DEBUGON)
  dtostrf((((float)s16Tmp+0.5) * (ADCREF/1024.0)), 4, 2, strUA1);
#endif

  // Power calculation
  // See https://ww2.minicircuits.com/pages/s-params/ZX47-55+_VIEW.pdf
  // Approached linear function f(x) = m*x + b from table "Output Voltage vs. Input Power @+25°C @2000MHz" on page 1
  // 1,88V -> -50dBm / 0,54V -> 5dBm
  fPwrdBm  = (-2750.0/67.0) * (((float)s16Tmp+0.5) * (ADCREF/1024.0)) + (1820.0/67.0);

  if ( s16Tmp > 583 ) // > 1,88V/< -50dBm
  {
    sprintf(strPwrW,"RF low! ");
    sprintf(strPwrdBm," --.- dBm");
  }
  else
  {
    if ( s16Tmp < 167 ) // < 0,54V/ > 5dBm
    {
      sprintf(strPwrW,"RF ovl! ");
      sprintf(strPwrdBm," --.- dBm");
    }
    else
    {
      fPwrdBm += (float)u8Att;

      dtostrf(fPwrdBm, 5, 1, strTmp);
      sprintf(strPwrdBm,"%5s dBm", strTmp);
    
      // Convert W from dBm and autorange
      if ( fPwrdBm < -30.0 )  // < 1uW: 0.1nW .. 999.9nW
      {
        dPwrW   = pow(10, (fPwrdBm/10.0)) * 1e6; // Calculate RF power in nW
        dtostrf(dPwrW, 5, 1, strTmp);
        sprintf(strPwrW,"%5s nW", strTmp);
      }
      else
      {
        if ( fPwrdBm < 0.0 )  // < 1mW: 0.1uW .. 999.9uW 
        {
          dPwrW   = pow(10, (fPwrdBm/10.0)) * 1e3; // Calculate RF power in uW
          dtostrf(dPwrW, 5, 1, strTmp);
          sprintf(strPwrW,"%5s uW", strTmp);
        }
        else
        {
          if ( fPwrdBm < 30.0 )  // < 1W: 0.1mW .. 999.9mW 
          {
            dPwrW   = pow(10, (fPwrdBm/10.0));    // Calculate RF power in mW
            dtostrf(dPwrW, 5, 1, strTmp);
            sprintf(strPwrW,"%5s mW", strTmp);
          }
          else                  // > 1W: 1W .. 999.9W
          {
            dPwrW   = pow(10, (fPwrdBm/10.0)) * 1e-3;    // Calculate RF power in W
            dtostrf(dPwrW, 5, 1, strTmp);
            sprintf(strPwrW,"%5s W ", strTmp);        
          }
        }
      }
    }
  }

    
  // ===== Temperature =====  
  // Read temperature equivalent voltage and filter
  as16UTempRaw[u8IdxUTempRaw] = analogRead(AI_AMBTEMP);  // Read temperature equivalent voltage (raw ADC)
  
  if ( ++u8IdxUTempRaw > ( (sizeof(as16UTempRaw) / 2) - 1 ) )
  {
    u8IdxUTempRaw = 0;  // Reset index
  }

   // Accumulate array and average
  s16Tmp = 0;
  for ( u8Count = 0; u8Count < (sizeof(as16UTempRaw) / 2); u8Count++ )
  {
    s16Tmp += as16UTempRaw[u8Count];
  }
  
  s16Tmp = s16Tmp / (sizeof(as16UTempRaw) / 2);

#if defined(DEBUGON)
  dtostrf((((float)s16Tmp+0.5) * (ADCREF/1024.0)), 4, 2, strUA0);
#endif

  // Temperature calculation 
  // See https://ww2.minicircuits.com/pages/s-params/ZX47-55+_VIEW.pdf
  // Approached linear function f(x) = m*x + b from table "Temperature Sensor Voltage Vs Ambient" on page 4
  // 0,44V -> -55°C / 0,77V -> 100°C
  fTamb = (15500.0/33.0) * (((float)s16Tmp+0.5) * (ADCREF/1024.0)) - (785.0/3.0);

  // Build string
  if ( fTamb < -55.0 )  // Temp too low
  {
    sprintf(strTempAmb,"<-55 %cC", (char)176);
  }
  else
  {
    if ( fTamb > 100.0 )  // Temp too high
    {
      sprintf(strTempAmb,">100 %cC", (char)176);
    }
    else
    {
      dtostrf(fTamb, 3, 0, strTmp);
      sprintf(strTempAmb," %3s %cC", strTmp, (char)176);
    }
  }

  // ===== Attenuation =====
  sprintf(strAtt,"%2d dB", u8Att);    
   
  updateScreen();
}


void updateScreen(void)
{
  u8g2.clearBuffer(); // Clear the internal display memory

  u8g2.setFont(u8g2_font_10x20_tf);

  // Power readings
  // Watts
  u8g2.setCursor(0, 28);
  u8g2.print(strPwrW);
  // dBm
  u8g2.setCursor(0, 46);
  u8g2.print(strPwrdBm);

  u8g2.setFont(u8g2_font_9x15_tf);        

#if defined(DEBUGON)

  // A0 voltage
  u8g2.setCursor(93, 10);
  u8g2.print(strUA0);

  // Pushbutton state
  u8g2.setCursor(93, 28);
  u8g2.print(strPBstate);
  
  // A1 voltage
  u8g2.setCursor(93, 46);
  u8g2.print(strUA1);

#endif
  
  // Attenuation
  u8g2.setCursor(0, 64);
  u8g2.print(F("ATT ")); u8g2.print(strAtt);

  // Version
  u8g2.setCursor(93, 64);
  u8g2.print(VERSION);
 
  // Temperature
  u8g2.setCursor(0, 10);        
  u8g2.print(F("T ")); u8g2.print(strTempAmb);

  u8g2.sendBuffer();  // Transfer internal memory to the display

return;
}


void showSplash(void)
{
    u8g2.setCursor(35, 13);
    u8g2.print(F("ZX47 RF"));
    u8g2.setCursor(15, 29);
    u8g2.print(F("Powermeter"));
    u8g2.setCursor(10, 45);
    u8g2.print(F("-55..5 dBm"));
    u8g2.setCursor(45, 64);
    u8g2.print(VERSION);

return;
}
