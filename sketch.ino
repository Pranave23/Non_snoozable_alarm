/*
  Many people struggle to wake up at their desired time, which often leads to a challenging and unproductive day.
  This difficulty can affect their mood, energy levels, and overall performance. To address this issue, this alarm system
  stimulator offers a practical solution. Unlike traditional alarms that can be snoozed, this system ensures that users
  wake up fully by requiring physical movement to turn it off. By incorporating features like motion detection, it encourages
  users to get out of bed, making the process of waking up easier and more effective. This approach aims to promote a
  more energetic start to the day, enhancing daily productivity.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Keypad.h>

#define PIR_PIN    11
#define BUZZER_PIN 13

LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS1307 rtc;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

int  alarmHour   = 7;
int  alarmMinute = 0;
bool alarmSet    = false;

// FIX 4: moved motionStartTime here (global) so it can be properly
//        reset both inside and after activateAlarm()
unsigned long motionStartTime = 0;
unsigned long alarmStartTime  = 0;

// Tracks last displayed second to avoid constant LCD rewrites
int lastDisplayedSecond = -1;

// ─── setup ───────────────────────────────────────────────────────────────────
void setup()
{
  pinMode(PIR_PIN,    INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin())
  {
    lcd.setCursor(0, 0);
    lcd.print("RTC not found");
    while (1);
  }

  if (!rtc.isrunning())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Alarm Time");
  lcd.setCursor(0, 1);
  lcd.print("Enter HHMM on pad");
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop()
{
  // ── Keypad input ──────────────────────────────────────────────────────────
  char key = keypad.getKey();
  if (key)
  {
    if (key >= '0' && key <= '9')
    {
      // FIX 3: removed unused 'inputCount' variable
      static String inputTime = "";

      inputTime += key;
      lcd.setCursor(0, 0);
      lcd.print("Input: ");
      lcd.print(inputTime);
      lcd.print("    ");   // clear any leftover characters

      if (inputTime.length() == 4)
      {
        int enteredHour   = (inputTime[0] - '0') * 10 + (inputTime[1] - '0');
        int enteredMinute = (inputTime[2] - '0') * 10 + (inputTime[3] - '0');

        if (enteredHour >= 0 && enteredHour < 24 &&
            enteredMinute >= 0 && enteredMinute < 60)
        {
          alarmHour   = enteredHour;
          alarmMinute = enteredMinute;
          alarmSet    = true;

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Alarm Set for:");
          lcd.setCursor(0, 1);
          if (alarmHour < 10)   lcd.print("0");
          lcd.print(alarmHour);
          lcd.print(":");
          if (alarmMinute < 10) lcd.print("0");
          lcd.print(alarmMinute);
          delay(2000);
          lcd.clear();
          lastDisplayedSecond = -1;   // force time refresh after clear
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Invalid Time!");
          delay(2000);
          lcd.clear();
          lastDisplayedSecond = -1;
        }
        inputTime = "";
      }
    }
  }

  // ── Display current time (update only when second changes) ────────────────
  DateTime now = rtc.now();
  if (now.second() != lastDisplayedSecond)
  {
    lastDisplayedSecond = now.second();
    lcd.setCursor(0, 0);
    lcd.print("Time: ");
    printTime(now.hour(), now.minute(), now.second());

    // Show alarm time on row 1 if set
    if (alarmSet)
    {
      lcd.setCursor(0, 1);
      lcd.print("Alarm: ");
      if (alarmHour < 10)   lcd.print("0");
      lcd.print(alarmHour);
      lcd.print(":");
      if (alarmMinute < 10) lcd.print("0");
      lcd.print(alarmMinute);
      lcd.print("       ");
    }
  }

  // ── Check alarm ───────────────────────────────────────────────────────────
  if (alarmSet && now.hour() == alarmHour && now.minute() == alarmMinute)
  {
    activateAlarm();

    // FIX 1: clear the alarm flag so it doesn't re-trigger every loop
    //        iteration for the rest of the same minute
    alarmSet = false;

    lcd.clear();
    lastDisplayedSecond = -1;   // force full redraw after alarm
  }
}

// ─── printTime ───────────────────────────────────────────────────────────────
void printTime(int hour, int minute, int second)
{
  if (hour   < 10) lcd.print("0");
  lcd.print(hour);
  lcd.print(":");
  if (minute < 10) lcd.print("0");
  lcd.print(minute);
  lcd.print(":");
  if (second < 10) lcd.print("0");
  lcd.print(second);
}

// ─── activateAlarm ───────────────────────────────────────────────────────────
void activateAlarm()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ALARM! MOVE TO STOP!");

  alarmStartTime  = millis();
  motionStartTime = 0;               // FIX 4: always start fresh

  // FIX 2 & 3: lastMotionTime is still static (fine — it only throttles
  //            the update rate), but the motionStartTime reset is now
  //            separated so it only resets on genuine LOW, not during
  //            the 500 ms debounce window.
  static unsigned long lastMotionTime = 0;

  while (millis() - alarmStartTime < 60000)
  {
    tone(BUZZER_PIN, 1000);           // passive buzzer: tone() works in Wokwi

    int motionDetected = digitalRead(PIR_PIN);

    if (motionDetected == HIGH)
    {
      // ── FIX 2: handle debounce WITHOUT touching motionStartTime ──────────
      if (millis() - lastMotionTime > 500)
      {
        lastMotionTime = millis();
      }

      // Start the sustained-motion timer on the first HIGH reading
      if (motionStartTime == 0)
      {
        motionStartTime = millis();
      }

      lcd.setCursor(0, 1);
      lcd.print("Motion Detected!    ");

      // Show countdown progress on row 2
      unsigned long elapsed = millis() - motionStartTime;
      lcd.setCursor(0, 10000);
      lcd.print("stay: ");
      lcd.print((int)(elapsed / 1000));
      lcd.print("s      ");

      if (elapsed >= 2000)
      {
        // ── Alarm dismissed ──────────────────────────────────────────────
        noTone(BUZZER_PIN);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Good Morning!");
        lcd.setCursor(0, 1);
        lcd.print("Have a nice day!");
        delay(3000);

        // FIX 4: reset motion timer before returning
        motionStartTime = 0;
        return;
      }
    }
    else
    {
      // ── FIX 2: motionStartTime reset ONLY on genuine LOW ─────────────
      motionStartTime = 0;

      lcd.setCursor(0, 1);
      lcd.print("No Motion!          ");
      lcd.setCursor(0, 2);
      lcd.print("                    ");
    }
    delay(500);
  }

  // Timeout — no motion for 60 s
  noTone(BUZZER_PIN);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("No Motion Detected");
  lcd.setCursor(0, 1);
  lcd.print("Alarm timed out.");
  delay(2000);

  // FIX 4: reset motion timer on timeout path too
  motionStartTime = 0;
}


