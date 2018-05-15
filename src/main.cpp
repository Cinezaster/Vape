#define ENCODER_OPTIMIZE_INTERRUPTS

#include <MicroView.h>
#include <ACS712.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Encoder.h>
#include <Bounce2.h>

#define ENCODER_PIN_1   3
#define ENCODER_PIN_2   2
#define ENCODER_BTN_PIN 5
#define BTN_PIN_1       A1
#define BTN_PIN_2       A2
#define HEATER_PIN      6

MicroViewWidget *widget;

Encoder myEnc(ENCODER_PIN_1, ENCODER_PIN_2);

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
ACS712 sensor(ACS712_20A, A0);

Bounce encoderButton = Bounce();
Bounce leftButton = Bounce();
Bounce rightButton = Bounce();

float I;
float avarageI;
unsigned long startSensorShow;
unsigned long startMeltTime;
unsigned long startVapeTime;
unsigned long startLockTime;
double temperature;
double ambientTemperature;
unsigned long screenRefreshRate;
signed int programState;
signed int menuState;
bool BTN_1 = HIGH;
bool BTN_2 = HIGH;
bool BTN_E = false;
float sumI = 0;
long countI = 0;

long allowedMeltTime = 9000;
long allowedVapeTime = 6000;
long lockTimeout = 5000;

float pwmGain = 0.25;
float maxPwmGain = 1;
signed int meltTemperature = 80;
signed int meltPwm = meltTemperature * pwmGain;
signed int maxMeltPwm = meltTemperature * maxPwmGain;
signed int vapeTemperature = 110;
signed int vapePwm = vapeTemperature * pwmGain;
signed int maxVapePwm = vapePwm * maxPwmGain;

void checkButtons() {
  if (encoderButton.update()) 
  {
    if (encoderButton.fell()) 
    {
      BTN_E = true;
    }
  }
  
  if (leftButton.update() || rightButton.update()) 
  {
    BTN_1 = leftButton.read();
    BTN_2 = rightButton.read();
  }
}

void getCurrent() {
  I = abs(sensor.getCurrentDC());
  sumI = sumI + I;
  countI++;
  if (countI >= 20) 
  {
    avarageI = sumI/countI;
    sumI = 0;
    countI = 0;
  }
}

void showMeasurements () {
  if (millis() > startSensorShow + 250) // read/calculate sensors every 0.5 seconds
  {
    if (programState == 2 || programState == 3)
    {
      uView.setCursor(0,23);
      uView.print(int(avarageI));
      uView.print("A");
    }

    // readsensor
    temperature = mlx.readObjectTempC();
    widget->setValue(temperature);
    uView.display();

    startSensorShow = millis();
  }
}

void setup() {

  /* Setup of pins */
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, false);

  pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_1, INPUT_PULLUP);
  pinMode(BTN_PIN_2, INPUT_PULLUP);

  encoderButton.attach(ENCODER_BTN_PIN);
  encoderButton.interval(40);
  leftButton.attach(BTN_PIN_1);
  leftButton.interval(40);
  rightButton.attach(BTN_PIN_2);
  rightButton.interval(40);

  /* Setup of Amp Sensor */
  int zero = sensor.calibrate();
  sensor.setZeroPoint(zero);
  
  /* Setup of Temperature Sensor */
  mlx.begin();

  /* Setup of Screen */
  uView.begin();                      // start MicroView
  uView.clear(PAGE);                  // clear page

  widget = new MicroViewSlider(0, 2, 0, 200, WIDGETSTYLE1);
  //widget2 = new MicroViewSlider(0, 20, 0, 20, WIDGETSTYLE1);

  delay(200);

  ambientTemperature = mlx.readAmbientTempC();

}

void loop() {

  checkButtons();
  getCurrent();

  showMeasurements ();
  

  switch (programState) {
    case 0: // waiting state
      {
        if (BTN_E) // if encoder button goto menu state
        { 
          programState = 1;
          menuState = 0;
          BTN_E = false;
        }

        if ((BTN_1 && !BTN_2) || (!BTN_1 && BTN_2)) // if one button go to melting state
        { 
          programState = 6;
        }

        if (!BTN_1 && !BTN_2) // if both button go to vaping state
        { 
          programState = 7;
        }
        break;
      }
    case 1: // menu state
      if (BTN_E) // if encoder button goto menu state
      { 
        menuState++;
        BTN_E = false;
      }
      switch (menuState) {
        case 0: // Prepare for set melt temperature
          myEnc.write(meltTemperature);
          menuState = 1;

          uView.clear(ALL);
          uView.display();
          break;
        case 1: // Set melt temperature
          meltTemperature = constrain(myEnc.read(), 40, 120);
          maxMeltPwm = meltTemperature * maxPwmGain;
          meltPwm = meltTemperature * pwmGain;

          if (meltTemperature == 40 || meltTemperature == 120)
          {
            myEnc.write(meltTemperature);
          }

          uView.setCursor(1,23);
          uView.print("Melt Temp:   ");
          uView.setCursor(1,33);
          uView.print(meltTemperature);
          uView.print(" C  ");
          uView.display();
          break;
        case 2: // Prepare for set vape temperature
          myEnc.write(vapeTemperature);
          menuState = 3;
          
          uView.clear(ALL);
          uView.display();
          break;
        case 3: // Set vape temperature
          vapeTemperature = constrain(myEnc.read(), 100, 180);
          maxVapePwm = vapeTemperature * maxPwmGain;
          vapePwm = vapeTemperature * pwmGain;

          if (vapeTemperature == 180 || vapeTemperature == 100)
          {
            myEnc.write(vapeTemperature);
          }

          uView.setCursor(1,23);
          uView.print("Vape Temp:   ");
          uView.setCursor(1,33);
          uView.print(vapeTemperature);
          uView.print(" C  ");
          uView.display();
          break;
        case 4:
          programState = 5;
          
          uView.clear(ALL);
          uView.display();
          break;
      }
      break;
    case 2: // Melting state
      {
        if (!BTN_1 && !BTN_2) // If both buttons go to Vaping state
        { 
          // Goto 3 over 7
          programState = 7;
          break;
        }

        if (BTN_1 && BTN_2) // If no buttons go to 0 state
        { 
          // Goto 0 over 5
          programState = 5;
          break;
        }

        if (startMeltTime + allowedMeltTime < millis()) // if heating to long --> lock
        {  
          // Goto 4 over 8
          programState = 8;
          break;
        }

        int setPwm = map(temperature, ambientTemperature, meltTemperature, maxMeltPwm, meltPwm);
        int finalPwm = constrain(setPwm, 0, maxMeltPwm);
        analogWrite(HEATER_PIN, finalPwm);

        uView.setCursor(32,23);
        uView.print(finalPwm);
        uView.print("pwm ");
        uView.display();
        
      }

      break;
    case 3: // Vape stage
      {
        if (BTN_1 || BTN_2) // If no buttons go to 0 state
        {
          // Goto 0 over 5
          programState = 5;
          break;
        }

        if (startVapeTime + allowedVapeTime < millis()) // if heating to long --> lock
        {
          // Goto 4 over 8
          programState = 8;
          break;
        }
        
        int setPwm = map (temperature, ambientTemperature, vapeTemperature, maxVapePwm, vapePwm);
        int finalPwm = constrain(setPwm, 0, maxVapePwm);
        uView.setCursor(32,23);
        uView.print(finalPwm);
        uView.print("pwm ");
        uView.display();
        analogWrite(HEATER_PIN,finalPwm);

        break;
      } 
    case 4: // Locked state
      {
        if (startLockTime + lockTimeout < millis()) // wait till locktime has passed
        {
          if (BTN_1 && BTN_2)
          {
            programState = 5;
          }
        }
        break;
      }
    case 5: // Display state before going to 0 state
      {
        analogWrite(HEATER_PIN,0);
        
        programState = 0;

        uView.setCursor(0,23);
        uView.print("            ");
        uView.setCursor(0,33);
        uView.print("            ");
        uView.display();
        break;
      }
    case 6: // Display state before going to Melt state
      {
        programState = 2;

        uView.setCursor(0,23);
        uView.print("            ");
        uView.setCursor(0,33);
        uView.print("Melting     ");
        uView.display();

        startMeltTime = millis();
        break;
      }
    case 7: // Display state before going to Vaping state
      {
        programState = 3;

        uView.setCursor(0,23);
        uView.print("            ");
        uView.setCursor(0,33);
        uView.print("Vaping      ");
        uView.display();

        startVapeTime = millis();
        break;
      }
    case 8: // Display state before going to Locked state
      {
        analogWrite(HEATER_PIN,0);
        
        startLockTime = millis();
        programState = 4;
        
        uView.setCursor(0,23);
        uView.print("            ");
        uView.setCursor(0,33);
        uView.print("Locked      ");
        uView.display();
        
        break;
      }
    case 9: // Display state before going to Fail state
      uView.clear(ALL);
      uView.setCursor(0,33);
      uView.print("ERROR");
      uView.display();
      programState = 10;
    case 10: // Fail State
      break;
  }
}
