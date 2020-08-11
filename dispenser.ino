#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <Wire.h>
#include <string.h>

// debug motors mode, comment after finding the pins
//#define testMotorsDebug

#define startCredits 25
#define STEPS 2048
#define revolutions_to_dispense 1
#define max_dispense 20
#define stepperSpeed 15
#define COIN 19
#define numberOfItems 7
// inverts relay outputs, HIGH means off relay
#define invertForRelay

// show bootscreen, comment if you dont want
#define start_screen
#define start_delay 1000  // 2.5 seconds

// for debugging only in setup()
#define debug_loop

/*array index assignement*/
#define pencil 0
#define index 1
#define tape 2
#define ballpen 3
#define letter 4
#define longbond 5
// #define a3paper 6

/*ASSIGNMENT NOTES

(LED, PINBTN)

A3, 39 //pencil
A4, 41 //index
A5, 43 //tape
47, 45 //ballpen
A7, A0 //letter
A9, A2 // long
A8, A1 //paperA3

PIN A6 seems dead for LED Pin

*/

/*------------BUTTON PINS------------*/
#define btnPencil A10
#define btnIndex A9
#define btnTape 43
#define btnBallpen 45
#define btnLetter 47
#define btnLong 49
//#define btnA3 A2
uint8_t buttonArray[] = {btnPencil, btnIndex, btnTape, btnBallpen,
                         btnLetter, btnLong};

/*------------led pins-------------*/
#define ledPencil A0
#define ledIndex A1
#define ledTape A2
#define ledBallpen A3
#define ledLetter A4
#define ledLong A5
//#define ledA3 A8
uint8_t ledArray[] = {ledPencil, ledIndex, ledTape, ledBallpen,
                      ledLetter, ledLong};
/*------STEPPERS -------------*/
// Pencils
byte pencilStepperPins[4] = {28, 26, 24, 22};  // 22,24,26,28

// Index
byte indexPins[4] = {29, 27,25 , 23};  // 23,25,27,29

// Tape
byte TapePin[4] = {36, 34, 32, 30};  // 30,32,34,36


// if reversed 37, 35, 33, 31
// Ballpen
byte BPenPin[4] = {37, 35, 33, 31};  // 31,33,35,37

// // Letter
// byte LetterPin[4] = {38, 40, 42, 44};  // 38,40,42,44

// // Long
// byte LongPin[4] = {39, 41, 43, 45};

// // A3
// byte a3pins[4] = {47, 49, 51, 53};

/*-----PRINTER MOTOR PINS---------*/

#define ForwardPrint_Letter 12
#define ReverseHead_Letter 10
#define ForwardHead_Letter 9

#define ForwardPrint_Legal 11
#define ReverseHead_Legal 8
#define ForwardHead_Legal 7

/*---------------LIST VARIABLES-----------------*/
/*
        "Pencil"      20
        "Index Card"  20
        "Tape",       20
        "Ballpen"     20
        "Letter"      1
        "Long"        1
        "A3"          3
*/
// For easier Serial or LCD printing
String itemByID[numberOfItems] = {"Pencil",      //
                                  "Index Card",  //
                                  "Tape",        //
                                  "Ballpen",     //
                                  "Letter",      //
                                  "Long",        //
                                  "A3"};         //

// Prices per item
int productByValue[numberOfItems] = {20,  // pencil
                                     20,  // Index
                                     20,  // tape
                                     20,  // ballpen
                                     1,   // letter
                                     1,   // long
                                     3};  // a3

// Start with zero credits, volatile because interrupt can change these values
// anytime
volatile int credits = startCredits;
volatile bool inserted = false;

/*------------------ KEYPADS-------------------*/
// pragma just helps sort code
#pragma region Keypads_section
const byte ROWS = 4;  // four rows
const byte COLS = 4;  // four columns
// define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                             {'4', '5', '6', 'B'},
                             {'7', '8', '9', 'C'},
                             {'*', '0', '#', 'D'}};
byte colPins[ROWS] = {2, 14, 15, 16};  // connect to the row pinouts of the keypad
byte rowPins[COLS] = {6, 5, 4, 3};  // column pinouts of the keypad

// initialize an instance of class NewKeypad
Keypad customKeypad =
    Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

#pragma endregion Keypads_section

LiquidCrystal_I2C lcd(
    0x27, 20,
    4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

/*--------------------FUNCTIONS SECTION IGNORE----------------------*/
#pragma region FUNCTIONS_SECTION

bool paperWasPressed = false; // it has to be here for coinInterrupt to read it
int papersDispensed = 0;

// attached to the coin pin and increments the credits whenever something is
// detected. It also flags the data that was inserted so the lcd may update as
// deemed necessary
void coinInterrupt() {
    if(!paperWasPressed){
        credits++;
        inserted = true;
    }
}

// fully clear a row not the whole lcd
void clearRow(uint8_t row) {
    lcd.setCursor(0, row);                           // set cursor to the start
    for (size_t i = 0; i < 20; i++) lcd.print(" ");  // start printing spaces
    lcd.setCursor(0, row);  // returns cursor to the start
}
// clear a row from the start of a column, ex. Amount: xxxxxxxxxx xxxxxxxx
void clearRowFrom(uint8_t row, uint8_t col) {
    lcd.setCursor(col, row);  // initial cursor position
    for (size_t i = col; i < 20; i++)
        lcd.print(" ");       // 20 is the # of columns of the lcd
    lcd.setCursor(col, row);  // return cursor
}
// gets user input compete with backspace function, isCrediting function is
// experimental and unused, will leave that here
int getUserInput(String msg, uint8_t row, bool isOneCharOnly,
                 bool isCrediting = false, uint8_t crediting_row = 2) {
    char customKey;
    String value = "";
    String msgCredit = "Credits: ";

    clearRow(row);  // clear the row to be able to refresh amount
    lcd.print(msg);

    // EXPERIMENTAL UNUSUED BLOCK OF IF
    if (isCrediting) {
        lcd.setCursor(0, row - 1);
        lcd.print(msgCredit);
    }

    do {
        // lcd.setCursor(0, row);              // set drawing line
        customKey = customKeypad.getKey();  // get button press
        switch (customKey) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (isOneCharOnly)  // for product ID, what you press is what
                                    // you get
                    value = String(customKey);
                else  // for amount, add the numbers
                    value += String(customKey);
                break;
            case '#':  // backspace function
                value.remove(value.length() - 1);
                clearRowFrom(row, msg.length());
                break;

            case '*':  // confirm
                return value.toInt();
                break;

            default:
                break;
        }

        // UNUSED BLOCK
        if (isCrediting) {
            if (inserted) {
                lcd.setCursor(msgCredit.length(), crediting_row);
                lcd.print(credits);
                inserted = false;
                lcd.setCursor(0, row);
            }
        }

        lcd.setCursor(msg.length(), row);
        lcd.print(value);  // Ex. "Amount: 21"

    } while (customKey != '*');  // wait until confirm with '*'
    return value.toInt();
}
// Request confirm with * or # to cancel
bool getConfirm(String msg, uint8_t row) {
    char customkey;
    clearRow(row);
    lcd.setCursor(0, row);
    lcd.print(msg);

    do {
        lcd.setCursor(0, row);
        customkey = customKeypad.getKey();
        switch (customkey) {
            case '*':
                return true;
                break;
            case '#':
                return false;
                break;
            default:
                break;
        }
    } while (customkey != '*' || customkey != '#');
    return false;
}
// Sets screen menu, row1,2,3,4
void setScreen(String row1 = "", String row2 = "", String row3 = "",
               String row4 = "") {
    lcd.clear();
    String rowArr[4] = {row1, row2, row3, row4};
    for (size_t i = 0; i < 4; i++) {
        lcd.setCursor(0, i);
        lcd.print(rowArr[i]);
    }
}
// Sets screen menu for specific row only (row, message)
void setRow(uint8_t row, String msg) {
    clearRow(row);
    lcd.print(msg);
}
// Make steps format (pass sterpper driver used, pass the pins used, and the
// amount to dispense), then turn of after dispensing
void makeSteps(byte stepperPins[], int amount) {
    Stepper stepperA(STEPS, stepperPins[3], stepperPins[1], stepperPins[0],
                     stepperPins[2]);
    stepperA.setSpeed(stepperSpeed);
    stepperA.step(STEPS * revolutions_to_dispense * amount);
    for (size_t i = 0; i < 4; i++)
        digitalWrite(stepperPins[i], LOW);  // turn off power
}

// Compensates credits *for paper dispense

void creditsCompensate(){
    /*
    //initiate a new variable and copy the volatile Credits variable
    int currentCredits = credits;
    currentCredits = currentCredits - 1; // subtract credits by 2
    
    //copy temporary variable to new credits
    credits = currentCredits;

    //print credits
    lcd.setCursor(0, 1);
    lcd.print((String)credits);
    */

    //aggressive condition for bookmark
    paperWasPressed = true;
}

// Paper dispensing variables
#define Paper_Letter 0
#define Paper_Legal 1
#define Paper_A3 3

// Dispenses paper according to paper type (Paper_Letter, Paper_Legal, Paper_A3)
void makePaper(uint8_t PaperType, int amount) {
    detachInterrupt(digitalPinToInterrupt(COIN));
    papersDispensed = amount;

    int PinForwardPrint = 0;
    int PinReverseHead = 0;
    int PinForwardHead = 0;
    switch (PaperType) {
        case Paper_Letter:
            PinForwardPrint = ForwardPrint_Letter;
            PinReverseHead = ReverseHead_Letter;
            PinForwardHead = ForwardHead_Letter;
            break;
        case Paper_Legal:
            PinForwardPrint = ForwardPrint_Legal;
            PinReverseHead = ReverseHead_Legal;
            PinForwardHead = ForwardHead_Legal;
            break;
        case Paper_A3:
            // PinForwardPrint = ForwardPrint_A3;
            // PinReverseHead = ReverseHead_A3;
            // PinForwardHead = ForwardHead_A3;
            break;
        default:
            break;
    };

    // motors on
    #ifdef invertForRelay
        digitalWrite(PinForwardPrint,LOW);
    #else
        digitalWrite(PinForwardPrint, HIGH);
    #endif

    for (int i = 0; i < amount; i++) {
        // head move forward to ramm
        analogWrite(PinForwardHead, 255*.80);
        delay(200);
        digitalWrite(PinForwardHead, LOW);
        // LATCH TIME
        delay(200);  // wait for latch
        // move head back;
        digitalWrite(PinReverseHead, 255*.80);
        delay(80);
        digitalWrite(PinReverseHead, LOW);

        // allow paper out
        delay(800);
    }
    // motors off
    #ifdef invertForRelay
        digitalWrite(PinForwardPrint,255/2);
    #else
        digitalWrite(PinForwardPrint, LOW);
    #endif
    
    delay(500); // compensate for electrical noise
    //credits = credits -2;
}
void dispenseById(uint8_t ItemId, int amountToDispense = 1) {
    switch (ItemId) {
        case pencil:  // Pencil
            makeSteps(pencilStepperPins, amountToDispense);
            break;
        case index:  // index
            makeSteps(indexPins, amountToDispense);
            break;
        case tape:  // tape
            makeSteps(TapePin, amountToDispense);
            break;
        case ballpen:  // ballpen
            makeSteps(BPenPin, amountToDispense);
            break;
        case letter:  // LETTER
            creditsCompensate();
            makePaper(Paper_Letter, amountToDispense);
            break;
        case longbond:  // Long
            creditsCompensate();
            makePaper(Paper_Legal, amountToDispense);
            break;
        // case a3paper:  // A3
        //     makePaper(Paper_A3, amountToDispense);
        //     break;
        default:
            break;
    }
}



/*--------------------END FUNCTIONS SECTION -----------------------*/
#pragma endregion FUNCTIONS_SECTION

/*-------------------ARDUINO SECTION------------------------------*/
void setup() {
    Serial.begin(9600);  // initialize for debugging
    lcd.init();          // initialize the lcd
    lcd.backlight();  // turn on LCD backlight, remember to adjust contrast on
                      // the back with philips if di kita
    //// FOR COINSLOT
    pinMode(COIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(COIN), coinInterrupt, FALLING);

    // Turn all leds and button modes as input pullups and outputs
    for (int i = 0; i < sizeof(buttonArray); i++) {
        pinMode(buttonArray[i], INPUT_PULLUP);
        pinMode(ledArray[i], OUTPUT);
    }

    // PRINTER MOTORS SETUP

    // LETTTER
    pinMode(ForwardPrint_Letter, OUTPUT);
    pinMode(ReverseHead_Letter, OUTPUT);
    pinMode(ForwardHead_Letter, OUTPUT);

    // LEGAL
    pinMode(ForwardPrint_Legal, OUTPUT);
    pinMode(ReverseHead_Legal, OUTPUT);
    pinMode(ForwardHead_Legal, OUTPUT);
#ifdef invertForRelay
        digitalWrite(ForwardPrint_Letter, HIGH);
        digitalWrite(ForwardPrint_Legal, HIGH);
#endif

#ifdef ForwardPrint_A3
    pinMode(ForwardPrint_A3, OUTPUT);
    pinMode(ReverseHead_A3, OUTPUT);
    pinMode(ForwardHead_A3, OUTPUT);
#endif

#ifdef start_screen
    setScreen("WELCOME", "FW Version 1.2");
    //setScreen("Test if Firmware", "was flashed");
    delay(start_delay);
#endif

#ifdef testMotorsDebug
    // PIN 12, Print 1
    // 11,10
    // #define ForwardPrint 12
    // #define ReverseHead 10
    // #define ForwardHead 11

    // 9 Print 2
    // 8,7
    pinMode(12, OUTPUT);  // forward
    pinMode(11, OUTPUT);  // reverse
    pinMode(10, OUTPUT);  // unknown
    #define runTestMotors
    #ifdef runTestMotors
        #define ForwardPrint 9
        #define ReverseHead 7
        #define ForwardHead 8
            int PinForwardPrint = ForwardPrint;
            int PinReverseHead = ReverseHead;
            int PinForwardHead = ForwardHead;

            // motors on
            digitalWrite(PinForwardPrint, HIGH);

            for (size_t i = 0; i < 1; i++) {
                // head move forward to ramm
                digitalWrite(PinForwardHead, HIGH);
                delay(150);
                digitalWrite(PinForwardHead, LOW);
                // LATCH TIME
                delay(200);  // wait for latch
                // move head back;
                digitalWrite(PinReverseHead, HIGH);
                delay(80);
                digitalWrite(PinReverseHead, LOW);

                // allow paper out
                delay(200);
            }
            // motors off
            digitalWrite(PinForwardPrint, LOW);
    #endif
    while (1)
            ;

#endif

#ifdef debug_loop
    //-------------------------------------- void setup()
    // initialize the possible states of 7 leds corrresponding to the items
    bool ledState[numberOfItems];

    for (int i = 0; i < numberOfItems; i++) {
        digitalWrite(ledArray[i], HIGH);
        ledState[i] = true;
    }
    bool wasPressed = false;
    bool reset = true;
    // display Credits, then actual value of credits
    while (true) {
        if (reset) {  // only works on start, no need to redraw
            setScreen("Insert Credits: ");
        }

        reset = false;

        //-------------------------------------- void Loop()
        while (!reset) {
            // initialize, every loop assumes no button is pressed yet

            if(paperWasPressed){
                attachInterrupt(digitalPinToInterrupt(COIN), coinInterrupt, FALLING);
                delay(1000);
                
                credits = credits - papersDispensed - 1; //-1 is for compensation
                papersDispensed = 0; // clear out papers
                
                paperWasPressed = false;
            }

            lcd.setCursor(0, 1);
            lcd.print((String)credits);
            
            wasPressed = false;

            for (int i = 0; i < numberOfItems; i++) {
                // reads the button, reports False button is pressed, True if
                // not pressed
                ledState[i] = digitalRead(buttonArray[i]);

                // if the button is pressed, it reports LOW/False, add it in the
                // history that it was pressed
                if (!ledState[i]) {
                    wasPressed = true;
                }
            }

            // FINALIZE
            for (int i = 0; i < numberOfItems; i++) {
                // if there is a history that a button was pressed, turn off all
                // the button led that isnt pressed by inverting the read led
                // state (if not pressed, button reports HIGH, invert makes it
                // LOW and set it as digital write)
                if (wasPressed) {
                    digitalWrite(ledArray[i], !ledState[i]);  // sets all lights
                }

                // if everything is not pressed and there's no history of
                // presses, set everything on
                else {
                    if (credits >= productByValue[i]) {
                        digitalWrite(ledArray[i], ledState[i]);
                    } else {
                        digitalWrite(ledArray[i], !ledState[i]);
                    }

                    Serial.println("no button");
                }
            }
            if (wasPressed) {
                for (int i = 0; i < numberOfItems; i++) {
                    if (!ledState[i])  // reports which led was pressed
                    {
                        // debugging purposes what button was pressed
                        // can be commented out
                        // Serial.println("Button for : " + itemByID[i]);
                        lcd.clear();

                        // default amount is 1 from the time because usually the
                        // user will only dispense 1 item for non-paper
                        // materials
                        int amount = 1;

                        // dispense process
                        if (credits >= productByValue[i]) {
                            // set amount to reduce

                            // meaning if any of the papers were selected AND
                            // the credits can pay offthe value of 2 or more
                            // than the value, do these
                            if ((i == letter || i == longbond) &&
                                credits >= (2 * productByValue[i])) {
                                // This variable will help the loop if he choses
                                // max dispensing, or insufficient amount
                                bool conditionErrorOrInsuff = true;
                                do {
                                    // This goes first before setting the bool
                                    // to false since it accounts for what the
                                    // screen should show first or overwrite
                                    if (conditionErrorOrInsuff)
                                        setScreen((String)("Dispensing: " +
                                                           itemByID[i]),
                                                  "Press * to enter");
                                    // turn off the boolean flag
                                    conditionErrorOrInsuff = false;
                                    // actively get user input on line 3
                                    amount = getUserInput("Enter Amount: ", 3,
                                                          false);

                                    // If max dispense amount of ex. 20 was
                                    // entered, inform user
                                    if (amount > max_dispense) {
                                        // message to display
                                        setScreen("Error", "Cannot dispense",
                                                  "more than 20pcs");

                                        // turns on the flag
                                        conditionErrorOrInsuff = true;

                                        // give user enough time to read
                                        delay(1000);

                                    } else if (amount * productByValue[i] >
                                               credits) {
                                        Serial.println("Line 587");
                                        setScreen("", "Insufficient line 596" + itemByID[i]);
                                        delay(1000);
                                        conditionErrorOrInsuff = true;
                                    }
                                    // when the error flag is turned on, repeat
                                    // the loop to check user input
                                } while (conditionErrorOrInsuff);
                            }

                            // This is the point where the user has confirmed
                            setScreen("Dispensing: " + itemByID[i]);
                            dispenseById(i, amount);
                            // subtracts credits by the value and amount chosen
                            if (!paperWasPressed){ //subtract if non-Paper transaction was made
                                credits = credits - productByValue[i] * amount;
                            }
                            
                                
                        }

                        // insufficient process
                        else {
                            Serial.print("Line 606 ");
                            setScreen("", "Insufficient", "Line 606", itemByID[i]);
                            delay(1000);
                        }

                        Serial.println(itemByID[i]);
                        //
                        reset = true;

                        /// delay(1000);  // simulate dispensing; // commented
                        /// due to actual dispensing already implemented
                        lcd.clear();
                    }
                }
            }
        }
    }
    delay(100);
#endif
}

void loop() {}