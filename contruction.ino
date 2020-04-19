#include "TinyWireM.h"
#include "TinyOzOLED.h"
#include <avr/wdt.h>
#include <avr/sleep.h>

//16 col x 8 row

#define PIN_SET 3
#define PIN_RST 1
#define PIN_TEMP A2


enum E_BUTTON
{
  BTN_MID,
  BTN_UP,
  BTN_DOWN,
  BTN_NONE
};


enum E_CONTRACTION_STATE
{
  CONT,
  REST,
  NONE
};


long timer;
byte timer_h;
byte timer_m;
byte timer_s;

OzOLED oled;
byte page;

E_CONTRACTION_STATE con_state = CONT;
E_BUTTON btn;
int cont[5];
int rest[5];
float temperature = 0;
int adc;



void setup() {

  pinMode(PIN_SET, INPUT_PULLUP);
  pinMode(PIN_RST, INPUT_PULLUP);
  delay(2000);
  oled.init();
  oled.sendCommand(0xA1); // set Orientation
  oled.sendCommand(0xC8);

  initTimer1();
  PrintTemperatureTemplate();
}
void loop() {
  btn = ReadKey();
  StateMachine( btn);
  //wdt_reset();
}


// set system into the sleep state
// system wakes up when wtchdog is timed out
void system_sleep() {
  setup_watchdog();
  wdt_reset();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();
  sleep_mode();                        // System actually sleeps here
  sleep_disable();                     // System continues execution here when watchdog timed out
}

void WakeUp()
{
  while (1)
  {
    delay(100);
    btn = ReadKey();
    if (btn == BTN_NONE)
    {
      system_sleep();
    }
    else {
      break;
    }
  }
}


ISR(WDT_vect) {
  /* ReEnable the watchdog interrupt, as this gets reset when entering this ISR and automatically enables the WDE signal that resets the MCU the next time the  timer overflows */
  WDTCR |= (1 << WDIE);
}

void setup_watchdog() {

  // Disable all interrupts
  cli();

  /* Clear MCU Status Register. Not really needed here as we don't need to know why the MCU got reset. page 44 of datasheet */
  MCUSR = 0;

  /* Disable and clear all Watchdog settings. Nice to get a clean slate when dealing with interrupts */

  WDTCR = (1 << WDCE) | (1 << WDE);
  WDTCR = 0;

  // Setup Watchdog for interrupt and not reset, and a approximately 500ms timeout P.45
  WDTCR = (1 << WDIE) | (1 << WDP2) | (1 << WDP0);

  // Enable all interrupts.
  sei();
}


E_BUTTON ReadKey()
{
#define THRESHOLD 5
#define SAMPLE 10
  byte s = 0, r = 0;
  for (int i = 0; i < SAMPLE; i++)
  {
    s += digitalRead(PIN_SET);
    r += digitalRead(PIN_RST);
    adc += analogRead(PIN_TEMP);
    delay(20);
  }
  adc = adc / SAMPLE;

  if (s >= THRESHOLD && r < THRESHOLD)
  {
    return BTN_DOWN;
  }
  else if (s < THRESHOLD && r >= THRESHOLD)
  {
    return BTN_UP;
  }
  else if (s < THRESHOLD && r < THRESHOLD)
  {
    return BTN_MID;
  }
  else
  {
    return   BTN_NONE;
  }
}


float ReadTemp()
{
  int Vo;
  float R1 = 2252;
  float logR2, R2, T;
  float A = 1.484778004e-03, B = 2.348962910e-04, C = 1.006037158e-07;  // Steinhart-Hart and Hart Coefficients

  Vo = adc;
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (A + B * logR2 + C * logR2 * logR2 * logR2)); // Steinhart and Hart Equation. T  = 1 / {A + B[ln(R)] + C[ln(R)]^3}
  T =  T - 273.15;
  return T;
}




ISR(TIMER1_COMPA_vect)
{
  TCNT1 = 0;
  timer++;
  timer_s++;

  if (con_state == CONT)
  {
    cont[0]++;
  }
  else if (con_state == REST)
  {
    rest[0]++;
  }


  if (timer_s == 60)
  {
    timer_s = 0;
    timer_m++;
    if ( timer_m == 60)
    {
      timer_m = 0;
      timer_h++;
      if ( timer_m == 24)
      {
        timer_m = 0;
      }
    }
  }
}

static inline void initTimer1(void)
{ //1Hz interrupt
  TCCR1 |= (1 << CTC1);  // clear timer on compare match
  TCCR1 |= (1 << CS13) | (1 << CS12) | (1 << CS11); //clock prescaler 16384
  OCR1A = 122; // compare match value
  TIMSK |= (1 << OCIE1A); // enable compare match interrupt
}

void timer_to_str(char *c, int timer) {
  int t = timer % 3600;
  byte m = t / 60;
  byte s = t % 60;

  c[0] = '0' + m / 10;
  c[1] = '0' + m % 10;
  c[2] = ':';
  c[3] = '0' + s / 10;
  c[4] = '0' + s % 10;
}



void PrintTemperatureTemplate()
{
  char tmp[10];
  oled.clearDisplay();
  oled.printString("TEMPERATURE", 3, 0, 11);

}

void PrintTemperature(float temperature)
{
  char tmp[10];
  dtostrf(temperature, 3, 2, tmp);
  oled.printChar('o',15,3);
  oled.printBigNumber(tmp, 3, 4, 4);//row 4 col 3
}

void PrintContactionTemplate()
{
  char line0[17] = "<SET|CONT-|REST-";
  char line1[17] = "<RST|     |     ";

  oled.clearDisplay();
  oled.printString(line0, 0, 0, 16);
  oled.printString(line1, 0, 1, 16);
  ContractionTimesUpdate();
}

void ContractionTimesUpdate()
{
  char buf[17]   = "0: 00:00 | 00:00";
  for (int i = 0; i < 5; i++)
  {
    buf[0] = '0' + i;
    oled.printString(buf, 0, i + 2, 16);
    timer_to_str(buf, cont[i]);
    oled.printString(buf, 3, i + 2, 5);
    timer_to_str(buf, rest[i]);
    oled.printString(buf, 11, i + 2, 5);
  }
}

void PrintContactionTimer(int cont[], int rest[])
{
  char buf[6];
  if (con_state == CONT)
  {
    timer_to_str(buf, cont[0]);
    oled.printString(buf, 3, 2, 5);
  }
  else if ( con_state == REST)
  {
    timer_to_str(buf, rest[0]);
    oled.printString(buf, 11, 2, 5);
  }
}

void PrintContactionDif(int cont[], int rest[])
{
  char buf[6];

  if (rest[1] > rest[2])
  {
    oled.printChar('^', 15, 0);
  }
  else if (rest[1] < rest[2])
  {
    oled.printChar('v', 15, 0);
  }
  else
  {
    oled.printChar('-', 15, 0);
  }
  timer_to_str(buf, rest[1]);
  oled.printString(buf, 11, 1, 5);

  if (con_state == CONT)
  {
    if (cont[1] > cont[2])
    {
      oled.printChar('^', 9, 0);
    }
    else if (cont[1] < cont[2])
    {
      oled.printChar('v', 9, 0);
    }
    else
    {
      oled.printChar('-', 9, 0);
    }
    timer_to_str(buf, cont[1]);
    oled.printString(buf, 5, 1, 5);
  }
  else if ( con_state == REST)
  {
    if (cont[0] > cont[1])
    {
      oled.printChar('^', 9, 0);
    }
    else if (cont[0] < cont[1])
    {
      oled.printChar('v', 9, 0);
    }
    else
    {
      oled.printChar('-', 9, 0);
    }
    timer_to_str(buf, cont[0]);
    oled.printString(buf, 5, 1, 5);
  }
}


void StateMachine( E_BUTTON btn)
{
  switch (page)
  {
    case 0:
      {
        //temperature  = ConvertTemperature();
        temperature = ReadTemp();
        PrintTemperature(temperature);
        if (btn == BTN_UP)
        {
          page = 1;
          PrintContactionTemplate();
        }
        if (btn == BTN_MID)
        {
          oled.setPowerOff();
          system_sleep();
          //Continue

          WakeUp();
          oled.setPowerOn();
        }
        break;
      }

    case 1: //Contraction monitor
      {
        if (btn == BTN_DOWN) //SET
        {
          if (con_state == CONT)
          {
            con_state = REST;
          }
          else if (con_state == REST)
          {
            con_state = CONT;
            for (int i = 3; i >= 0; i--)
            {
              cont[i + 1] = cont[i];
              rest[i + 1] = rest[i];
            }
            cont[0] = 0;
            rest[0] = 0;
            ContractionTimesUpdate();
          }

        }
        else if (btn == BTN_UP) //RESET
        {
          for (int i = 0; i < 5; i++)
          {
            cont[i] = 0;
            rest[i] = 0;
          }
          ContractionTimesUpdate();
        }
        else if (btn == BTN_MID)
        {
          PrintTemperatureTemplate();
          page = 0; // TEMPERATURE
          break;
        }

        PrintContactionTimer(cont, rest);
        PrintContactionDif(cont, rest);
        break;
      }
    default:
      {
        page = 0;
      }
  }

  if (btn != BTN_NONE)
  {
    delay(500);
  }
}
