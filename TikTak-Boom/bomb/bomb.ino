enum BombMode {
  None,
  TickTak,
  Explosion
};

int speakerpin = 13; // динамик на +5 В
int button = 7; // кнопка на землю gnd
int vibro = 8;
byte len;
BombMode currentMode;

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
    digitalWrite(vibro, LOW)
  }

  if (currentMode == None && digitalRead(button) == LOW) {
    currentMode = TickTak;
    len = random(5, 26);  // 5 = 9  x = 45
  }
}
