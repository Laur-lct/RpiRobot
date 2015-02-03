#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
//#include "utils/json.h" //output json

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define AUTO_STOP_DELAY 2500
#define CYCLE_DELAY 20

// Pin number declarations. We're using the Broadcom chip pin numbers.
const int rMotorSpeedPin = 23;
const int rMotorDirPin = 24;
const int lMotorSpeedPin = 13;
const int lMotorDirPin = 19;
const int lightsPin = 26;

const char verbose = 1;
const int speedChangeStep = 8;

int lCurSpeed =0;
int rCurSpeed =0;
int lDesiredSpeed =0;
int rDesiredSpeed =0;

int autoStopAfter = AUTO_STOP_DELAY;

// wait for input in separate thread
PI_THREAD (myThread)
{
    int fd;
    char *myfifo ="/tmp/robotIn";
    unlink(myfifo);
    mkfifo(myfifo, 0777);
    fd = open(myfifo, O_RDONLY);
    if (verbose) printf("Opened FIFO\n");
    char c[3];
    while(1)
    {
        if (read(fd,c,3)==3)
        {
            if (abs(c[0]-128)<=100) lDesiredSpeed = c[0] - 128;
            if (abs(c[1]-128)<=100) rDesiredSpeed = c[1] - 128;
			//temp
			digitalWrite(lightsPin, c[2]!=0);
			
            if (verbose) printf("Got l=%i, r=%i.\n",lDesiredSpeed, rDesiredSpeed);
            autoStopAfter = AUTO_STOP_DELAY;
        }
        delay(CYCLE_DELAY);
    }
}

void setup()
{
    // Setup stuff:
    piHiPri(99);
    wiringPiSetupGpio(); // Initialize wiringPi -- using Broadcom pin numbers

    pinMode(lMotorDirPin, OUTPUT);
    pinMode(lMotorSpeedPin, OUTPUT);
    pinMode(rMotorDirPin, OUTPUT);
    pinMode(rMotorSpeedPin, OUTPUT);
	
	pinMode(lightsPin, OUTPUT);

    softPwmCreate(lMotorSpeedPin,0,100);
    softPwmCreate(rMotorSpeedPin,0,100);

    piThreadCreate (myThread);
    if (verbose) printf("SetupComplete\n");
}

int main(void)
{
    //set allow file exec
    umask (0);
    setup();
    while(1)
    {
        if (lCurSpeed != lDesiredSpeed)
        {
            if (lCurSpeed < lDesiredSpeed) lCurSpeed = min(lDesiredSpeed, lCurSpeed + speedChangeStep);
            else lCurSpeed = max(lDesiredSpeed, lCurSpeed - speedChangeStep);
            digitalWrite(lMotorDirPin, lCurSpeed>0);
            softPwmWrite(lMotorSpeedPin,abs(lCurSpeed));
        }
        if (rCurSpeed != rDesiredSpeed)
        {
            if (rCurSpeed < rDesiredSpeed) rCurSpeed = min(rDesiredSpeed, rCurSpeed + speedChangeStep);
            else rCurSpeed = max(rDesiredSpeed, rCurSpeed - speedChangeStep);
            digitalWrite(rMotorDirPin, rCurSpeed<0);
            softPwmWrite(rMotorSpeedPin,abs(rCurSpeed));
            if (verbose) printf("Current l=%i, r=%i.\n",lCurSpeed,rCurSpeed);
		}
        if (autoStopAfter<0 && (lDesiredSpeed!=0 || rDesiredSpeed !=0))
        {
            lDesiredSpeed=0;
			rDesiredSpeed=0;
            if (verbose) printf("Auto Stopping...");
        }
        else
            autoStopAfter-=CYCLE_DELAY;
        delay(CYCLE_DELAY);
    }

    return 0;
}
