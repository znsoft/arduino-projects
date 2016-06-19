/*
  Использовали резистор на 220 
  Порядок подключения: 
  GND - коротка нога светодиода
  8 пин - резистор - длинная нога светодиода
 */

int led1=8;
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin 13 as an output.

  pinMode(led1, OUTPUT);
  digitalWrite(led1, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(4000);
  digitalWrite(led1, LOW);    // turn the LED off by making the voltage LOW
  delay(500);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(led1, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(2000);              // wait for a second
  digitalWrite(led1, LOW);    // turn the LED off by making the voltage LOW
  delay(500);              // wait for a second
}
