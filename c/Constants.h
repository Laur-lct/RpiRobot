#define DEBUG 


#define AUTO_STOP_DELAY 2500
#define CYCLE_DELAY 20
#define CYCLES_PER_CALIBRATION 10  // calibration will occur every CYCLE_DELAY*CYCLES_PER_CALIBRATION msek
#define NAMED_PIPE_NAME "/tmp/robotIn"

// Pin number declarations. We're using the Broadcom chip pin numbers.
const char rMotorSpeedPin = 23;
const char rMotorDirPin = 24;
const char rEncoderPin = 28;
const char lMotorSpeedPin = 13;
const char lMotorDirPin = 19;
const char lEncoderPin = 3;

const char lightsPin = 26;

/* helper macro */
// a macro that executes one line of code only if DEBUG flag is set.
#if defined(DEBUG)
    #define DBG_ONLY(x) (x)   
#else
    #define DBG_ONLY(x) 
#endif

#ifndef max
	#define max(a,b) (((a) > (b)) ? (a) : (b))
	#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif