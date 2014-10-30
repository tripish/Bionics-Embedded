#include "BasicParameter.h"
/*
    10:30am 10/24/2014 - Got Train compiling and called to as an inheritance from Animation class
    7pm                - Train runs with Basic Parameters, controllable from internal loops
    9:35pm             - Integrated Sparkle with FastLED dimming to best abilities...no good, need to write own
    2PM     10/26/2014 - Several animations running great.  Fixed dimming.  Moving on to xBee integration.  Several sensor running.
    2:30PM             - xBee transmit and receive working at 15 ms / frame
    1PM     10/27/2014 - About to integrate TinyPacks
    1AM     10/28/2014 - Integrating TinyPacks with Xbee
*/
#include "FastLED.h"
#define NUM_LEDS 12

CRGB leds[NUM_LEDS];
CHSV ledsHSV[NUM_LEDS];

// Had to move this to main file for proper dependencies...
//#include "BasicParameter.h"
#include "Animations.h"

#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector



enum STATES {
    READ_SENSORS,
    ANIMATE,
    COMMUNICATE
};

inline STATES& operator++(STATES& _state, int)  // <--- note -- must be a reference
{
   const int i = static_cast<int>(_state);
   _state = static_cast<STATES>((i + 1) % 3);
   return _state;
}

enum STATES state = READ_SENSORS;

//enum ReportType {
//  REPORT_DATA,
//  REPORT_AVAIL_PARAMETERS
//};
//
//enum ReportType reportType = REPORT_AVAIL_PARAMETERS;
#define REPORT_DATA 0
#define REPORT_AVAIL_PARAMETERS 1

// ================================================================
// ===                    Animation SETUP                     ===
// ================================================================

unsigned long lastAnimate = 0;

#define NUM_ANIMATIONS 5

Train train;
Fire fire;
Sparkle sparkle;
Power power;
RunningRainbow runningrainbow;

enum AnimationState {
	FIRE,
	TRAIN,
	SPARKLE,
	POWER,
	RUNNINGRAINBOW
};

byte currentAnimation = POWER;

//#define AUTO_ANIMATION_CHANGER
#define animationSwitchPeriod 3000
unsigned long timeOfLastAnimationChange = 0;

// ================================================================
// ===                    XBEE SETUP                     ===
// ================================================================

#include <TinyPacks.h>

PackWriter writer;
PackReader reader;

#define MAX_TEXT_LENGTH 32
char text[MAX_TEXT_LENGTH] = "";

struct ParamControlMessage {
  char key[16];
  float val;
};




// TODO put in tx16 class
#define ACK_OPTION 0
#define DISABLE_ACK_OPTION 1
#define BROADCAST_OPTION 4

#include "XBee.h"

// xBee variables
XBee xbee = XBee();

#define MAX_PACKED_DATA 100
uint8_t packed_data[MAX_PACKED_DATA];
int packed_data_length;

/*
Transmission variables
Tx16Request(uint16_t addr16, uint8_t option, uint8_t *data, uint8_t dataLength, uint8_t frameId)

OPTIONS (whether or not local radio retries failed tranmissions)
    #define ACK_OPTION 0
    #define DISABLE_ACK_OPTION 1
    #define BROADCAST_OPTION 4

FRAME ID (whether or not target radio sends confirmation)
    #define DEFAULT_FRAME_ID 1
    #define NO_RESPONSE_FRAME_ID 0
*/

// Everything currently sent for no retries, no ACK
const uint16_t ADDRESS_COORDINATOR = 0x0001;
Tx16Request tx = Tx16Request( ADDRESS_COORDINATOR, DISABLE_ACK_OPTION, packed_data, sizeof(packed_data), NO_RESPONSE_FRAME_ID );
// Tx16Request tx = Tx16Request( 0x0001, ACK_OPTION, packed_data, sizeof(packed_data), DEFAULT_FRAME_ID );
TxStatusResponse txStatus = TxStatusResponse();
unsigned long timeOfLastTransmission = 0;;
#define transmissionPeriod 30 // 30 -> ~30fps


// Receiving variables
XBeeResponse response = XBeeResponse(); 
Rx16Response rx16 = Rx16Response(); // create reusable response objects for responses we expect to handle


// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {

    // Begin Serial
    Serial.begin(115200);
    // while (!Serial); // wait for Leonardo enumeration, others continue immediately

    delay(1000);
    
    Serial.println();
    Serial.println();
    Serial.println(F("--------------------"));
    Serial.println(F("--------------------"));
    Serial.println(F("BIONIC FRAMEWORK START"));
    Serial.println(F("--------------------"));
    Serial.println(F("--------------------"));
    Serial.println();

    Serial.println(F("Bionic Framework SETUP")); Serial.println();

    // Start xBee
    Serial.println("--");
    xbeeSetup();
    Serial.println(F("--xBee Setup Complete")); Serial.println();

    // Setup MPU
    Serial.println("--");
    MPUsetup();
    Serial.println(F("--MPU Setup Complete")); Serial.println();

    // Setup LEDs
    Serial.println("--");
    LEDsetup();
    Serial.println(F("--LED Setup Complete")); Serial.println();


    Serial.println(F("SETUP Complete"));
    Serial.println(F("--------------------"));
    Serial.println(F("--------------------")); Serial.println();
    delay(1000);


    // Initialize timers
    timeOfLastTransmission = millis();
    timeOfLastAnimationChange = millis();
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {

    Serial.println(F("Run loop"));

    switch ( state ) {

        case READ_SENSORS:
            Serial.println("----------");
            Serial.println("Start STATE MACHINE");
            MPUread();
            break;

        case ANIMATE:
            Serial.println("-----");
            LEDrun();
            break;

        case COMMUNICATE:
            Serial.println("-----");
            
            // LOAD INTO MESSAGE STRUCTURES?
            // Loop internally to collect as many messages as were sent? Maybe not necessary anymore...
            getCommunications();
            
            unsigned long timeSinceLastTransmission = millis() - timeOfLastTransmission;
            if ( timeSinceLastTransmission > transmissionPeriod ) {
                BasicParameter *p[2] = { &power.level_Parameter, &power.hue_Parameter };
                sendCommunications_Report( REPORT_DATA, p, 2);
                timeOfLastTransmission = millis();
            }

            // ACT ON THE MESSAGES HERE?
            // IF SO, SET THE ANIMATION PARAMETERS
            // DO DECAY FIRST
            // THEN HUE

            Serial.println("----------");
            Serial.println();
            break;

    }

    state++;


    // FOR TESTING - Animation changing timer
    #ifdef AUTO_ANIMATION_CHANGER
    unsigned long timeSinceLastAnimationChange = millis() - timeOfLastAnimationChange;
    if ( timeSinceLastAnimationChange > animationSwitchPeriod ) {
        Serial.println("ANIMATION CHANGE!");
        currentAnimation = (currentAnimation + 1) % NUM_ANIMATIONS;
        timeOfLastAnimationChange = millis();
    }
    #endif

    // Delay if you want to slow down the effective framerate of the unit
    // delay(10);

}
