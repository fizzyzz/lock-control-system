#include <Keypad.h>
#include <EEPROM.h>

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const byte PIN_IR_SENSOR = 9;
const byte PIN_SPEAKER   = 10;
const byte PIN_LED_GREEN = 11;
const byte PIN_LED_RED   = 12;
const byte PIN_BACKLIGHT = 13;

const byte CODE_LENGTH  = 4;
const byte MAX_ATTEMPTS = 3;
const int  EEPROM_ADDR  = 0;
const unsigned long LOCKOUT_MS = 10000;

char storedCode[CODE_LENGTH + 1];
char inputBuffer[CODE_LENGTH + 1];
byte inputPos = 0;
byte wrongAttempts = 0;
byte hashCount = 0;
const byte RESET_HASH_COUNT = 5;

void setup() {
  pinMode(PIN_IR_SENSOR, INPUT);
  pinMode(PIN_SPEAKER,   OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  Serial.begin(9600);
  keypad.setDebounceTime(50);
  loadCode();
  lockedState();
  Serial.println(F("Kodovyi zamok gotov. Vvedite kod:"));
}

void loop() {
  handleBacklight();
  char key = keypad.getKey();
  if (key) {
    beep(50);
    processKey(key);
  }
}

void loadCode() {
  bool valid = true;
  for (byte i = 0; i < CODE_LENGTH; i++) {
    char c = EEPROM.read(EEPROM_ADDR + i);
    if (c < '0' || c > '9') { valid = false; break; }
    storedCode[i] = c;
  }
  storedCode[CODE_LENGTH] = '\0';
  if (!valid) {
    strcpy(storedCode, "0000");
    saveCode(storedCode);
  }
}

void saveCode(const char *code) {
  for (byte i = 0; i < CODE_LENGTH; i++) {
    EEPROM.update(EEPROM_ADDR + i, code[i]);
  }
}

void processKey(char key) {
  if (key == '*') {
    hashCount = 0;
    changeCodeMenu();
    return;
  }
  if (key == '#') {
    resetInput();
    hashCount++;
    if (hashCount >= RESET_HASH_COUNT) {
      factoryReset();
      hashCount = 0;
      return;
    }
    Serial.print(F("Vvod sbroshen. '#' podryad: "));
    Serial.println(hashCount);
    return;
  }
  if (key >= '0' && key <= '9') {
    hashCount = 0;
    if (inputPos < CODE_LENGTH) {
      inputBuffer[inputPos++] = key;
      inputBuffer[inputPos] = '\0';
      Serial.print('*');
    }
    if (inputPos == CODE_LENGTH) {
      checkCode();
    }
  }
}

void factoryReset() {
  strcpy(storedCode, "0000");
  saveCode(storedCode);
  resetInput();
  Serial.println(F("\n!!! SBROS KODA: teper kod = 0000 !!!"));
  for (byte i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    tone(PIN_SPEAKER, 2000, 120);
    delay(160);
    digitalWrite(PIN_LED_GREEN, LOW);
    delay(160);
  }
  lockedState();
}

void checkCode() {
  if (strcmp(inputBuffer, storedCode) == 0) accessGranted();
  else                                      accessDenied();
  resetInput();
}

void accessGranted() {
  Serial.println(F("\nDostup razreshen!"));
  wrongAttempts = 0;
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);
  successTone();
  Serial.println(F("Zamok OTKRYT..."));
  delay(1250);
  lockedState();
  Serial.println(F("Zamok snova zapert. Vvedite kod:"));
}

void accessDenied() {
  wrongAttempts++;
  Serial.print(F("\nNevernyi kod. Popytka "));
  Serial.print(wrongAttempts);
  Serial.print('/');
  Serial.println(MAX_ATTEMPTS);
  errorTone();
  blinkRed(2);
  if (wrongAttempts >= MAX_ATTEMPTS) lockout();
}

void lockout() {
  Serial.println(F("BLOKIROVKA! Signalizaciya."));
  unsigned long start = millis();
  while (millis() - start < LOCKOUT_MS) {
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_SPEAKER, 1000);
    delay(300);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_SPEAKER);
    delay(300);
  }
  wrongAttempts = 0;
  lockedState();
  Serial.println(F("Blokirovka snyata. Vvedite kod:"));
}

void changeCodeMenu() {
  Serial.println(F("\n--- Smena koda ---"));
  Serial.println(F("Vvedite tekushchii kod (# - otmena):"));
  char current[CODE_LENGTH + 1];
  if (!readCodeBlocking(current)) return;
  if (strcmp(current, storedCode) != 0) {
    Serial.println(F("Kod neveren. Otmena."));
    errorTone();
    return;
  }
  Serial.println(F("Vvedite NOVYI kod:"));
  char newCode[CODE_LENGTH + 1];
  if (!readCodeBlocking(newCode)) return;
  Serial.println(F("Povtorite NOVYI kod:"));
  char confirm[CODE_LENGTH + 1];
  if (!readCodeBlocking(confirm)) return;
  if (strcmp(newCode, confirm) == 0) {
    strcpy(storedCode, newCode);
    saveCode(storedCode);
    Serial.println(F("Kod izmenen i sohranen v EEPROM!"));
    successTone();
  } else {
    Serial.println(F("Kody ne sovpadayut. Otmena."));
    errorTone();
  }
}

bool readCodeBlocking(char *dest) {
  byte pos = 0;
  while (pos < CODE_LENGTH) {
    handleBacklight();
    char key = keypad.getKey();
    if (key) {
      if (key == '*' || key == '#') {
        Serial.println(F("Otmena"));
        return false;
      }
      if (key >= '0' && key <= '9') {
        beep(50);
        dest[pos++] = key;
        dest[pos] = '\0';
        Serial.print('*');
      }
    }
  }
  Serial.println();
  return true;
}

void resetInput() {
  inputPos = 0;
  inputBuffer[0] = '\0';
}

void lockedState() {
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, HIGH);
  resetInput();
}

void handleBacklight() {
  digitalWrite(PIN_BACKLIGHT, digitalRead(PIN_IR_SENSOR) == HIGH ? HIGH : LOW);
}

void beep(int ms)      { tone(PIN_SPEAKER, 2000, ms); }

void successTone() {
  tone(PIN_SPEAKER, 1500, 150); delay(180);
  tone(PIN_SPEAKER, 2500, 300); delay(320);
  noTone(PIN_SPEAKER);
}

void errorTone() {
  tone(PIN_SPEAKER, 300, 250); delay(260);
  noTone(PIN_SPEAKER);
}

void blinkRed(byte times) {
  for (byte i = 0; i < times; i++) {
    digitalWrite(PIN_LED_RED, LOW);  delay(80);
    digitalWrite(PIN_LED_RED, HIGH); delay(80);
  }
}