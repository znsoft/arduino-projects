#include <avr/sleep.h>

enum BombMode {
  None,
  TickTak,
  Explosion
};

int buttonPressTimer;
int speakerpin = 13; // динамик на +5 В через конденсатор
int button = 2; // кнопка на землю gnd. для возможности выключения или включения это должен быть пин 2 или 3 только на них можно вешать прирывания
int vibro = 8;
byte len;
BombMode currentMode;


//Buzzer for arduino nano , there is no tone method on it
void buzz( long frequency, long length) {
  long delayValue = 1000000 / frequency / 2; // calculate the delay value between transitions
  //// 1 second's worth of microseconds, divided by the frequency, then split in half since
  //// there are two phases to each cycle
  long numCycles = frequency * length / 1000; // calculate the number of cycles for proper timing
  //// multiply frequency, which is really cycles per second, by the number of seconds to
  //// get the total number of cycles to produce
  for (long i = 0; i < numCycles; i++) { // for the calculated length of time...
    digitalWrite(speakerpin, HIGH); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
    digitalWrite(speakerpin, LOW); // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue); // wait again or the calculated delay value
  }
}


void melody1() {
  buzz( 2000, 200);
  buzz( 1200, 200);
  buzz( 3500, 100);
}

void melody2() {
  buzz( 1200, 200);
  buzz( 2500, 100);
}


void wakeUpNow()        // here the interrupt is handled after wakeup
{
  // execute code here after wake-up before returning to the loop() function
  // timers and code using timers (serial.print and more...) will not work here.
  // we don't really need to execute any special functions here, since we
  // just want the thing to wake up
}


void sleepNow()         // here we put the arduino to sleep
{
  /* Now is the time to set the sleep mode. In the Atmega8 datasheet
     http://www.atmel.com/dyn/resources/prod_documents/doc2486.pdf on page 35
     there is a list of sleep modes which explains which clocks and
     wake up sources are available in which sleep mode.

     In the avr/sleep.h file, the call names of these sleep modes are to be found:

     The 5 different modes are:
         SLEEP_MODE_IDLE         -the least power savings
         SLEEP_MODE_ADC
         SLEEP_MODE_PWR_SAVE
         SLEEP_MODE_STANDBY
         SLEEP_MODE_PWR_DOWN     -the most power savings

     For now, we want as much power savings as possible, so we
     choose the according
     sleep mode: SLEEP_MODE_PWR_DOWN

  */
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here

  sleep_enable();          // enables the sleep bit in the mcucr register
  // so sleep is possible. just a safety pin

  /* Now it is time to enable an interrupt. We do it here so an
     accidentally pushed interrupt button doesn't interrupt
     our running program. if you want to be able to run
     interrupt code besides the sleep function, place it in
     setup() for example.

     In the function call attachInterrupt(A, B, C)
     A   can be either 0 or 1 for interrupts on pin 2 or 3.

     B   Name of a function you want to execute at interrupt for A.

     C   Trigger mode of the interrupt pin. can be:
                 LOW        a low level triggers
                 CHANGE     a change in level triggers
                 RISING     a rising edge of a level triggers
                 FALLING    a falling edge of a level triggers

     In all but the IDLE sleep modes only LOW can be used.
  */

  attachInterrupt(button - 2, wakeUpNow, LOW); // use interrupt 0 (pin 2) and run function
  // wakeUpNow when pin 2 gets LOW

  sleep_mode();            // here the device is actually put to sleep!!
  // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP

  sleep_disable();         // first thing after waking from sleep:
  // disable sleep...
  detachInterrupt(button - 2);      // disables interrupt 0 on pin 2 so the
  // wakeUpNow code will not be executed
  // during normal running time.

}


void noise(long frequency, long length) {
  long delayValue = 1000000 / frequency ; // calculate the delay value between transitions
  //// 1 second's worth of microseconds, divided by the frequency, then split in half since
  //// there are two phases to each cycle
  long numCycles = frequency * length / 1000; // calculate the number of cycles for proper timing
  //// multiply frequency, which is really cycles per second, by the number of seconds to
  //// get the total number of cycles to produce
  for (long i = 0; i < numCycles; i++) { // for the calculated length of time...
    digitalWrite(speakerpin, random(2)); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
  }
}

void setup() {
  randomSeed(analogRead(0));
  pinMode(speakerpin, OUTPUT);
  pinMode(vibro, OUTPUT);
  pinMode(button, INPUT);
  digitalWrite(button, HIGH);
  currentMode = None;
}

void TikTak() {
  for (int i = 0; i < 3; i++) {
    noise( 20000, 10);
    delay(250);
    noise( 3000, 10);
    delay(250);
  }
}

void loop() {
  if (currentMode == TickTak) {
    len--;
    if (len == 0) {
      currentMode = Explosion;
    }
    TikTak();
  }

  if (currentMode == Explosion) {
    currentMode = None;
    digitalWrite(vibro, HIGH);
    noise( 2000, 1500);
    digitalWrite(vibro, LOW);
  }

  if (currentMode == None && digitalRead(button) == HIGH && buttonPressTimer > 0) { //digitalRead(button) == HIGH && buttonPressTimer > 0 - означает что кнопку отпустили

    buttonPressTimer = 0;
    currentMode = TickTak;

    len = random(5, 26);  // 5 = 9  x = 45

  }


  if (currentMode == None && digitalRead(button) == LOW) {

    buttonPressTimer++;
    if (buttonPressTimer > 400) {  //4 секунды удержание кнопки

      melody1();
      for (int i = 100; i > 0 ; i--) noise( 2500 - i * 20, 15);
      delay(1000);
      buttonPressTimer = 0;
      sleepNow();
      melody2();
    }
    delay(10);
  } else buttonPressTimer = 0;
}
