#include <Arduino.h>
#include <EEPROM.h>
#define SYSCLOCK 8000000
#define SAMPLE_SIZE  128
#define IR_SEND_PWM_START    TCCR1 |=  _BV(COM1A0)
#define IR_SEND_PWM_STOP    TCCR1 &= ~(_BV(COM1A0))

const uint16_t pwmval = (SYSCLOCK / 4000 / 36);

int setcounter = 0;
int IRpin = 2;
int IRLED = 0;
int DCDCen = 4;
unsigned int TimerValue[SAMPLE_SIZE];
byte change_count;
long time;
byte mOSCCAL;

// HAL
#define REDLED_ON digitalWrite(1,HIGH)
#define REDLED_OFF digitalWrite(1,LOW)

/*
Input:
TIMEOUT in seconds if no buttons pressed
Return:
0 - no buttons pressed
1 - Left Button pressed
2 - Right Button pressed
3 - Both buttons pressed
*/
int readButtons(int readtimeout){
int a = 0;
  readtimeout = readtimeout * 10;
  readtimeout++;
while (readtimeout > 0){
  delay(100);
  int but = analogRead(A3);
  if (but > 35) a = 1;
  if (but > 150) a = 2;
  if (but > 250) a = 3;
  if (a > 0)  readtimeout = 0;
  readtimeout--;
}
return a;
}

// Turn off power
void PowerOFF(void){
  delay(500);
  REDLED_OFF;
  digitalWrite(DCDCen,LOW);
 while (1);
}

//Flash Red Led a times
void LED_flash(int a){
  while (a > 0){
    delay(666);
  REDLED_ON;
  delay(333);
  REDLED_OFF;
  a--;
  }
}
// End of hal
void SendCommand(int command){

int memTCCR1 = TCCR1;

pinMode(IRLED, OUTPUT);
digitalWrite(IRLED,0);
delay(10);
// Command sending begin:
TCCR1 = _BV(PWM1A)  | _BV(CS12);
OCR1C = pwmval; OCR1A = (pwmval / 3) * 2;
IR_SEND_PWM_STOP;

REDLED_OFF;



  // write to eeprom 0-0xff, 0x100-0x1ff, 0x200-0x2ff
  int addr = (command - 1) * 0x100;

  change_count = 0;
  while (change_count < SAMPLE_SIZE) {
  byte highByte = EEPROM.read(addr++);
  byte lowByte = EEPROM.read(addr++);

  TimerValue[change_count++] = ((highByte << 8) & 0xFF00) + lowByte;
}

cli();
OSCCAL = mOSCCAL;
change_count = 0;
while (change_count < SAMPLE_SIZE) {

// set pwm on/off
    if (TimerValue[change_count] & 0x8000)
      {
      IR_SEND_PWM_STOP;
      if (TimerValue[change_count] == 0xffff) break;
      TimerValue[change_count] &= 0x7fff;

      }
      else
      {
      IR_SEND_PWM_START;
      }


// do delay

      TIFR |= _BV(TOV1);
        while (TimerValue[change_count])
        {
          if (bitRead(TIFR,TOV1))      //(TIFR & _BV(TOV1))
            {
              TimerValue[change_count]--;
              TIFR |= _BV(TOV1);
            }
        };
      change_count++;
  }




IR_SEND_PWM_STOP;
  REDLED_OFF;
TCCR1 = memTCCR1;
sei();

  delay(200);

}

void LearnCommands(int readtimeout){
int direction = 0;


delay(5);
REDLED_ON;
//wait to release buttons
  while (readButtons(0) > 0);
// read which buttons command to learn
int button = readButtons(10);
REDLED_OFF;
if (button == 0) PowerOFF();  // if no buttons pressed -> power off
else {
// indicate button#
 LED_flash(button);
}
delay(500);

cli();

// Command learning begin:
TCCR1 = _BV(PWM1A)  | _BV(CS12);// | _BV(CS10);
OCR1C = pwmval; OCR1A = (pwmval / 3) * 2;

pinMode(IRLED,INPUT);

pinMode(IRpin, INPUT_PULLUP);
//while(digitalRead(IRpin) == LOW);
IR_SEND_PWM_STOP;
change_count = 0;

OSCCAL = mOSCCAL;
REDLED_ON;
// wait for start frame
while(digitalRead(IRpin) == HIGH);


  while (change_count < SAMPLE_SIZE) {

    TimerValue[change_count] = 0;
    TIFR |= _BV(TOV1);
// low time
      while(digitalRead(IRpin) == LOW)
        {
          if (bitRead(TIFR,TOV1))
            {
              TimerValue[change_count]++;
              TIFR |= _BV(TOV1);
            }
        };
      change_count++;
//high time
    TimerValue[change_count]=0x8000;
    TIFR |= _BV(TOV1);
      while(digitalRead(IRpin) == HIGH)
      {
        if (bitRead(TIFR,TOV1))      //(TIFR & _BV(TOV1))
          {
            TimerValue[change_count]++;
            TIFR |= _BV(TOV1);
            if (TimerValue[change_count] > 0x9000)
            {
              while (change_count < SAMPLE_SIZE )
                        TimerValue[change_count++] = 0xFFFF;
                              break;
            }
          }
      };

      change_count++;
    }
  // code received
// store code button = 1 || 2 || 3
change_count = 0;
// write to eeprom 0-0xff, 0x100-0x1ff, 0x200-0x2ff
int addr = (button - 1) * 0x100;

while (change_count < SAMPLE_SIZE)
      {
      EEPROM.write(addr++,TimerValue[change_count]>>8);
      EEPROM.write(addr++,TimerValue[change_count++] & 0xff);

      }

REDLED_OFF;
PowerOFF();
}


void setup() {

  // put your setup code here, to run once:
 mOSCCAL = OSCCAL;
  pinMode(DCDCen, OUTPUT);
  digitalWrite(DCDCen,HIGH);
  pinMode(IRpin, INPUT);
  pinMode(3, INPUT);
  pinMode(1, OUTPUT);
  pinMode(IRLED, INPUT);
  digitalWrite(IRLED,0);
  //TCCR0A = 0;           // turn off frequency generator (should be off already)
  //TCCR0B = 0;           // turn off frequency generator (should be off already)
  //TCCR1A = _BV(COM0A0); TCCR0B = _BV(WGM01) | _BV(CS00);
  pinMode(IRpin, INPUT_PULLUP);
  delay(5);

//  delay(50);
  REDLED_OFF;
  delay(5);
  IR_SEND_PWM_STOP;
delay(50);
int a = readButtons(0);
delay(350);
if (a < 3)
SendCommand(a);

}

void loop() {
  // put your main code here, to run repeatedly:
//delay(1000);
int a = readButtons(0);
if (a == 0) PowerOFF();
else {
      if (a == 3) {
        setcounter++;
        delay(1000);
      }
      else SendCommand(a);
      if (setcounter > 4) LearnCommands(10);

  }
}
