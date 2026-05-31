/*
--------------------------------------------------
Project : Real-Time OLED Embedded Dashboard
Author  : Nandineesh Tripathi
Platform: Arduino Uno

Features:
- ADC input via potentiometer
- PWM output control
- SSD1306 OLED dashboard
- UART diagnostics
- Interrupt-based emergency button
- Non-blocking scheduler
- FSM page navigation
- EEPROM page persistence
--------------------------------------------------
*/

//Installing libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
//initialising the OLED screen:
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//Variables we need to define
volatile bool pressed=false;
bool paused=false;
unsigned long lastInterruptTime=0;
const unsigned long debounceDelay=100;
unsigned long InterruptTime1=0;
unsigned long InterruptTime2=0;
int storedPage;
// Variables for the data of this system we have the struct SystemData where the variables related to data like ADC value and PWM output
struct SystemData{
  int adc_value; //ADC value which we will get from the potentiometer
  int pwm_output; //Equivalent PWM output we need for the LED
};

//similarly we have a struct for times where the variables related to time are mentioned
struct SystemTime{
  unsigned long ctime=0; //current time
  unsigned long ptime1=0; // previous value of time at different task are reffered as ptimeX here
  unsigned long ptime2=0;
  unsigned long ptime3=0;
  unsigned long ptime4=0;
};

//similarly we have a structure for Navigations and buttons
struct SystemNavigatingButtons{
  int PreviousButtonState; //Previous state of next button 
  int CurrentButtonState; // Current state of next button
  int PrevPButtonState; // previous state of previous button
  int CurrentPButtonState; // current state of next button
};

SystemData data;
SystemNavigatingButtons buttons;
SystemTime time;

enum Page{ //We created this enum for different pages of the OLED system
  HOME_PAGE, // here it will show the value of ADC vale and PWM output
  UART_PAGE, //On this page we will be having UART debugging related things 
  STATUS_PAGE, // this page will define overall status of the system like brightness,font etc
  PAGE_COUNT // this store the total number of pages we have in the OLED screen 
};
Page currentPage;
//Now we have defined all those variables it is time for defining the task here

// The first task which we have is PWM ADC task here we will read the ADC value of potentiometer and will give the equivalent output PWM value to the LED.
void pwm_adc_task(){
  data.adc_value=analogRead(A0); //Reading the value of Potentiometer pin i.e. A0
  data.pwm_output=(data.adc_value/4); // Converting that ADC value to the PWM value by deviding it by 4 as ADC range is 4 times the range of PWM.
  analogWrite(10,data.pwm_output); // Providing the converted PWM value to LED Pin that is pin 10.
}

void uart_debug(){ // This is our UART debug function this will print the values of PWM and ADC and working status of OLED
  Serial.print("ADC value: ");
  Serial.println(data.adc_value);
  Serial.print("PWM value: ");
  Serial.println(data.pwm_output);
  Serial.println("OLED is working");
}
//Now we have OLED tasks like switching of page and displaying the values we have to split the OLED functions in different segments 
void oled(){ // this is our general function of page switching 
  if (currentPage==HOME_PAGE){
    oled_homePage();
  }
  else if(currentPage==UART_PAGE){
    oled_UARTPage();
  }
  else if(currentPage==STATUS_PAGE){
    oled_StatusPage();
  }
}

void oled_homePage(){//This is homepage our OLED screen here we will have ADC value and PWM value shown on the screen 
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("ADC Value: ");// ADC input value of Potentiometer
  display.println(data.adc_value);
  display.print("PWM Value: "); //PWM output Value of LED
  display.println(data.pwm_output);
  display.display();
}
void oled_StatusPage(){// This is status page and it will display status of the machine like brightness,font and battery
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Brightness: Auto"); 
  display.println("Battery: Charged");
  display.print("Pen Colour: White");
  display.display();  
}

void oled_UARTPage(){ // this is UART debug page
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("ADC- "); //ADC input value of potentiometer
  display.println(data.adc_value);
  display.print("PWM- "); //PWM output value of LED
  display.println(data.pwm_output);
  display.println("OLED is working"); //Working status of OLED
  display.display();
}

void paused_screen(){ //This is an emergency mode screen here the system will be paused for some time when the black button is pressed
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("EMERGENCY MODE");
  display.println("System Paused");
  display.display();
}
/* Now the all the tasks are over now we have need a scheduler to schedule our task in regular time intervals */
void scheduler(void (*func)(), int x,unsigned long * timer){
  if (millis()-*timer>=x){
    func(); //Its function is that when a sufficient ammount of time passes it has to execute the function again
    *timer=millis();
  }
}
//We don't want the system to continuosly check for the emergency button instead of that we want that when button is pressed it stop doing its other task and get to the emergency mode for this we will use interrupt 
void ISR_function(){
  unsigned long interruptTime=millis();
  if(interruptTime-lastInterruptTime>debounceDelay){//here we are preventing the system from reading multiple press at once
    pressed=true;
    lastInterruptTime=interruptTime;
  }
}

void setup(){ // this is our arduino setup function
  pinMode(2, INPUT_PULLUP);  // we have our emergency button attached to pin number 2 
  pinMode(3, INPUT_PULLUP); // we have our Next button connected to pin number 3
  pinMode(4, INPUT_PULLUP); // we have our Previous button connected to pin number 4
  /* We have used pull up resistance all above just to remove the floating input and set the default value to 1 */
  Serial.begin(9600); //Started our serial board with a communication speed of 9600 bits per second herr
  pinMode(10, OUTPUT); //This is LED we have attached it to pin number 10
  pinMode(A0, INPUT); // Potentiometer is connected to pin A0
  storedPage = EEPROM.read(0); //We are also using EEPROM in this system to store the page we were at when system was at power last time
  Serial.print("EEPROM Page = ");
  Serial.println(storedPage);
  currentPage = (Page)storedPage;
  if(currentPage >= PAGE_COUNT)
  {
    currentPage = HOME_PAGE;
  }
  attachInterrupt(digitalPinToInterrupt(2), ISR_function, FALLING); //assigning pin 2 as interrupt
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  oled();
  buttons.PreviousButtonState = HIGH;
  buttons.PrevPButtonState = HIGH;    
}
//Now the setup is done let us move on the loop
void loop(){
  time.ctime=millis(); //reading the current time

  buttons.CurrentButtonState=digitalRead(3); //This reads the state of Next button 
  buttons.CurrentPButtonState=digitalRead(4); // this reads the state of previous button

  if(buttons.CurrentButtonState==0 && buttons.PreviousButtonState==1){
  if(time.ctime-InterruptTime1>debounceDelay){// preventing multiple button press readings
    currentPage=(Page)(currentPage+1)%PAGE_COUNT; //Changing the page to next page
    EEPROM.update(0, (int)currentPage); // storing the next page in EEPROM
    InterruptTime1=time.ctime; 
    }
  }

  if(buttons.CurrentPButtonState==0 && buttons.PrevPButtonState==1){
    if(time.ctime-InterruptTime2>debounceDelay){ //same logic as above
      currentPage=(Page)((currentPage+2)%PAGE_COUNT); // changing the page to next page
      EEPROM.update(0,(int)currentPage); // storing the page in EEPROM
      InterruptTime2=time.ctime;
    }
  }

  if(pressed && !paused){//WE made the function to enter the paused state of the system
  paused=true;
  time.ptime4=time.ctime;
  pressed=false;
  analogWrite(10,0); //turning the LED off
  Serial.println("Emergency Pause Activated"); // Sending the state through serial board
  }

  if(paused){ // pause state 
    paused_screen(); // it will show our paused screen

    if(time.ctime-time.ptime4>=2000){
      paused=false; // after 2 seconds it will again be back to normal mode
      Serial.println("System Resumed");
    }
  }

  else{ // we are executing the task of normal mode here
    scheduler(pwm_adc_task,20,&time.ptime1);
    scheduler(oled,150,&time.ptime2);
    scheduler(uart_debug,500,&time.ptime3);
  }
  buttons.PreviousButtonState=buttons.CurrentButtonState; //Storing the button state of the Next button
  buttons.PrevPButtonState=buttons.CurrentPButtonState; // storing the button state of the Previous button
}