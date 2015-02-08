#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include "Constants.h"
//#include "utils/json.h" //output json

char lReverse =0; // 0 -fwd, 1 backwd
char rReverse =0;
short lDesiredPower =0;
short rDesiredPower =0;
short lRealPower =0;
short rRealPower =0;

unsigned int lCurSpeed =0;
unsigned int rCurSpeed =0;

unsigned short cycleNum = 0;

int autoStopAfter = AUTO_STOP_DELAY;

void SetMotorsValue(unsigned char lPercent,unsigned char rPercent, bool isReverseL,bool isReverseR)
{
	lReverse = isReverseL;
	rReverse = isReverseR;
	
	lDesiredPower = min(100,lPercent);
	lRealPower = lDesiredPower;

	rDesiredPower = min(100,rPercent);
	rRealPower = rDesiredPower;
	//drop enc value
	lCurSpeed=0;
	rCurSpeed=0;
	
	digitalWrite(lMotorDirPin, lReverse);
	softPwmWrite(lMotorSpeedPin,lRealPower);
	digitalWrite(rMotorDirPin, !rReverse);
	softPwmWrite(rMotorSpeedPin,rRealPower);
	DBG_ONLY(printf("Current l=%i, r=%i.\n",lRealPower,rRealPower));
	
	autoStopAfter = AUTO_STOP_DELAY;
}

// wait for input in separate thread
PI_THREAD (myThread)
{
    int fd;
    char *myfifo = NAMED_PIPE_NAME;
    unlink(myfifo);
    mkfifo(myfifo, 0777);
    fd = open(myfifo, O_RDONLY);
    DBG_ONLY(printf("Opened FIFO\n"));
    char c[3];
    while(1)
    {
        if (read(fd,c,3)==3)
        {
			int l = c[0]-128;
			int r = c[1]-128;
			SetMotorsValue(abs(l),abs(r),l < 0,r < 0);
            //temp
			digitalWrite(lightsPin, c[2]==1);
        }
        delay(CYCLE_DELAY);
    }
}

void LEncoderHandler() {lCurSpeed++;}
void REncoderHandler() {rCurSpeed++;}

void setup()
{
    //set allow file exec
    umask (0);
    piHiPri(99);
    wiringPiSetupGpio(); // Initialize wiringPi -- using Broadcom pin numbers

    pinMode(lMotorDirPin, OUTPUT);
    pinMode(lMotorSpeedPin, OUTPUT);
	wiringPiISR(lEncoderPin,INT_EDGE_FALLING,LEncoderHandler);
	
    pinMode(rMotorDirPin, OUTPUT);
    pinMode(rMotorSpeedPin, OUTPUT);
	wiringPiISR(rEncoderPin,INT_EDGE_FALLING,REncoderHandler);
	
	pinMode(lightsPin, OUTPUT);

    softPwmCreate(lMotorSpeedPin,0,100);
    softPwmCreate(rMotorSpeedPin,0,100);

    piThreadCreate (myThread);
    DBG_ONLY(printf("SetupComplete\n"));
}

bool SyncMotors()
{
    if (lCurSpeed + rCurSpeed == 0)
	    return false;
  float ratioDiff = (float)lCurSpeed / (lCurSpeed + rCurSpeed) - (float)lDesiredPower / (lDesiredPower + rDesiredPower);
  char absDiff1 = lRealPower - lDesiredPower;
  char absDiff2 = rRealPower - rDesiredPower;
  bool isSameSign = (absDiff1 ^ absDiff2) >= 0;
  char changeAmount = (abs(ratioDiff)>0.25 ? 3 : (abs(ratioDiff)>0.10 ? 2 : 1));
  DBG_ONLY(printf("ratioDiff=%f\n",ratioDiff));
  //lmotor speed compared to rmotor speed is slower then desired
  if (ratioDiff < -0.01f){
    // here we have two options - either speed up l or slow down r.
    // we choose that change, which will keep overall real power closer to desired value
    // e. g. any motor will always try to keep its power as close to desired as possible.
    if (lRealPower < 100 && ((!isSameSign && absDiff1 <= absDiff2) || (isSameSign && absDiff1 >= absDiff2)))
      lRealPower+=min(changeAmount,100-lRealPower);
    else
      rRealPower-=min(changeAmount,rRealPower);
    return true;
  }
  //lmotor speed compared to rmotor speed is faster then desired
  else if (ratioDiff > 0.01f){
    // same two options - either slow down l or speed up r.
    if (rRealPower == 100 || (isSameSign && absDiff1 <= absDiff2) || (!isSameSign && absDiff1 >= absDiff2))
      lRealPower-=min(changeAmount,lRealPower);
    else
      rRealPower+=min(changeAmount,100-rRealPower);
    return true;
  }
  return false;
}

void UpdateMotors()
{
	bool changed = false;
	if (lDesiredPower == rDesiredPower && lDesiredPower >5)
	{	
		DBG_ONLY(printf("Calibrating... speed l=%i,r=%i\n", lCurSpeed,rCurSpeed));
		changed |= SyncMotors();
	}
	if (lRealPower < lDesiredPower && rRealPower < rDesiredPower)
	{
		lRealPower++;
		rRealPower++;
		changed=true;
	}
	else if (lRealPower > lDesiredPower + 1 && rRealPower > rDesiredPower + 1) 
	{
		lRealPower--;
		rRealPower--;
		changed=true;                
	}
	if (changed)
	{
		digitalWrite(lMotorDirPin, lReverse);
		softPwmWrite(lMotorSpeedPin,lRealPower);
		digitalWrite(rMotorDirPin, !rReverse);
		softPwmWrite(rMotorSpeedPin,rRealPower);
	}
}

int main(void)
{
    setup();
    while(1)
    {
		if (autoStopAfter<0 && (lDesiredPower!=0 || rDesiredPower !=0))
		{
			SetMotorsValue(0,0,lReverse,rReverse);
			DBG_ONLY(printf("Auto Stopped..."));
		}
		else autoStopAfter-=CYCLE_DELAY;
		if (cycleNum >= CYCLES_PER_CALIBRATION)
		{
			UpdateMotors();
			cycleNum=0;
		}
        delay(CYCLE_DELAY);
		cycleNum++;
    }

    return 0;
}
