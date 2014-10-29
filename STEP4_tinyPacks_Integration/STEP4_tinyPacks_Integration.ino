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

#include "BasicParameter.h"
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



// ================================================================
// ===                    Animation SETUP                     ===
// ================================================================

unsigned long lastAnimate = 0;

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

AnimationState currentAnimation = POWER;


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


#include "XBee.h"

// xBee variables
XBee xbee = XBee();

#define MAX_PACKED_DATA 100
uint8_t packed_data[MAX_PACKED_DATA];
int packed_data_length;

// Transmission variables
Tx16Request tx = Tx16Request(0x0001, packed_data, sizeof(packed_data)); // 16-bit addressing: Enter address of remote XBee, typically the coordinator
TxStatusResponse txStatus = TxStatusResponse();

// Receiving variables
XBeeResponse response = XBeeResponse(); 
Rx16Response rx16 = Rx16Response(); // create reusable response objects for responses we expect to handle
uint8_t option = 0;

// Had to move here in order to compile...prefer it in xBee_functions
 void sendCommunications( void (*_loadTx)() )
//void sendCommunications()
{
    // Pack data into transmission
    // These are custom mapped from Power animation
    // NEED TO UNDO THIS...
    (*_loadTx)();
//    loadUnitReport();

    // Send data to main BASE
    xbee.send(tx);

    // Query xBee for incoming messages
    if (xbee.readPacket(1))
    {
        if (xbee.getResponse().isAvailable()) {
            if (xbee.getResponse().getApiId() == RX_16_RESPONSE)
            {
                xbee.getResponse().getRx16Response(rx16);
                Serial.println("rx response");
            }
            else if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) Serial.println("tx response");
        }
        else
        {
            // not something we were expecting
            // Serial.println('xbee weird response type');    
        }
    }
    else
    {
        Serial.println(F("No tx ack from xBee"));
    }
}

 void sendCommunicationsParamReport( void (*_loadTx)(BasicParameter *), BasicParameter *_basicParameter )
//void sendCommunications()
{
    // Pack data into transmission
    // These are custom mapped from Power animation
    // NEED TO UNDO THIS...
    (*_loadTx)(_basicParameter);
//    loadUnitReport();

    // Send data to main BASE
    xbee.send(tx);

    // Query xBee for incoming messages
    if (xbee.readPacket(1))
    {
        if (xbee.getResponse().isAvailable()) {
            if (xbee.getResponse().getApiId() == RX_16_RESPONSE)
            {
                xbee.getResponse().getRx16Response(rx16);
                Serial.println("rx response");
            }
            else if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) Serial.println("tx response");
        }
        else
        {
            // not something we were expecting
            // Serial.println('xbee weird response type');    
        }
    }
    else
    {
        Serial.println(F("No tx ack from xBee"));
    }
}




// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {

    // Begin Serial
    Serial.begin(115200);
    // while (!Serial); // wait for Leonardo enumeration, others continue immediately
    
    Serial.println(F("Bionic Framework Setup"));

    // Start xBee
    xbeeSetup();
    Serial.println(F("xBee Setup Complete"));

    // Setup MPU
    MPUsetup();
    Serial.println(F("MPU Setup Complete"));

    // Setup LEDs
    LEDsetup();
    Serial.println(F("LED Setup Complete"));


    Serial.println(F("Setup Complete"));
    Serial.println(F("----------"));
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {

    // Serial.println(F("Run loop"));

    switch ( state ) {

        case READ_SENSORS:
            MPUread();
            break;

        case ANIMATE:
            LEDrun();
            break;

        case COMMUNICATE:
           sendCommunications(packTx_Report);
//           sendCommunications();
            // getCommunications();
            break;

    }

    state++;

    // delay(10);

}