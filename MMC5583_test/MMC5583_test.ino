#include <Wire.h>
#include <math.h>

// the MCM5883MC uses 0x30 as its i2c address. 
// this was on of the clues that helped me to identify the chip
// on the GY-801 
#define MMC_ADDRESS 0x30  // I2C address of MMC5883MC



void setup()
{
  // a character to hold the chip identifier
  // this was another way I positively identified the chip
  unsigned char c;

  // serial so we can see the data
  Serial.begin(115200);
  // wire for the I2C
  Wire.begin();

  // here 0x8 is being written to register 0x8 to set the
  // built in electro magnet to set up the poles
  // this sort of works like a reset
  // for those new to i2c let me explain what is going on to start.
  
  // this command sends a start pulse followed by the chip address as the first 7 bits
  // and then it puts a 0 bit for WRITE. It then waits for an acknolegment
  // from the chip 
  Wire.beginTransmission(MMC_ADDRESS);
  // the address of the register is written first
  // in this case CONTROL REGISTER 0
  Wire.write(0x08);
  // then the data is written. I nthis case a 1 in the 4th position or hex 0x8
  Wire.write(0x08);
  // end transmission sents a STOP to indicate the end of the WRITE
  Wire.endTransmission();

  // now as a verification read the device ID
  
  Wire.beginTransmission(MMC_ADDRESS);
  // write a 2f to the register to read
  Wire.write(0x2f);
  // send a STOP WRITE
  Wire.endTransmission();
  // this tells the chip to send 1 byte from that register
  // in i2c this is a START READ
  Wire.requestFrom(MMC_ADDRESS, 1);
  // we then wait for 1 byte to be recieved
  while(Wire.available()<1);
  // once the byte arrives we read it.
  c = Wire.read();
  //send another STOP READ
  Wire.endTransmission();
  // print out the ID which is 0x0C
  Serial.print("ID = ");
  Serial.println(c);
  // the data zero and range have to be determined.
  // there are six pieces to gather calibrate does this
  calibrate();
  //delay (30000);  
}

long xMax = 0;
long xMin = 0;
long yMax = 0;
long yMin = 0;
long zMax = 0;
long zMin = 0;

void readData()
{
  // reading the data is the same I could have made it a function
  // so skip down to evaluating the data
  
  unsigned short xLSB;
  unsigned short xMSB;
  unsigned short yLSB;
  unsigned short yMSB;
  unsigned short zLSB;
  unsigned short zMSB;
  long sx;
  long sy;
  long sz;
  float x;
  float y;
  float z;
  int c = 0;
  float angle;
  static int count = 0;
  Wire.beginTransmission(MMC_ADDRESS);
  Wire.write(0x08);
  Wire.write(1);
  Wire.endTransmission();
  while((c & 1) == 0)
  {
    Wire.beginTransmission(MMC_ADDRESS);
    Wire.write(7);
    Wire.endTransmission();
    Wire.requestFrom(MMC_ADDRESS, 1);
    while(Wire.available()<1);
    c = Wire.read();
    Wire.endTransmission();
  }
  Wire.beginTransmission(MMC_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(MMC_ADDRESS, 6);
  while(Wire.available()<6);
  xLSB = Wire.read();
  xMSB = Wire.read();
  yLSB = Wire.read();
  yMSB = Wire.read();
  zLSB = Wire.read();
  zMSB = Wire.read();
  
  Wire.endTransmission();
  sx = (long)(xMSB << 8) + xLSB;
  sy = (long)(yMSB << 8) + yLSB;
  sz = (long)(zMSB << 8) + zLSB;
  
  //***************************************************************************
  //  Evaluation time.
  //***************************************************************************
  // this read convert the data from garbage to a value from -1.0 to +1.0
  // this says subrtact the min from the value to shift it to a number starting at zero
  // subtract the min from the max to create a range
  // devide the shifted number by that range to give a percent from xero to one
  // multiply by 2 and subtract 1 giving a number -1.0 to +1.0
  // the reason this works is simple...
  // for the x axis when it is pointed north it is at its minimal value
  // when it is pointed north it is at its max so north is zero south is -1.0
  // from x only you can't tell east or west
  // y is 1.0 pointing west and -1.0 pointing east
  // z is one pointed at the center of the earth and -1 pointing away
  // these three combined have converted the values to a UNIT SPhere or a sphere with
  // radius of one. 
  
  x = 2.0*(float)(sx - xMin)/(float)(xMax - xMin) - 1.0;
  y = 2.0*(float)(sy - yMin)/(float)(yMax - yMin) - 1.0;
  z = 2.0*(float)(sz - zMin)/(float)(zMax - zMin) - 1.0;
  // now that we have x any in units we need for trigonometry
  // we can create a compass
  // if you are wanting to make a compass with the chip parallel to the ground
  // you only have to spin it in the x-y plane z doesnt matter.
  // when you know the opposite Y and the adjacent X you use arctangent to ge the angle
  
  // arc tangen is only good for one half the circle. It repeats itself in the other half.
  // the function is mirrored on the diagonal (math talk) 
  // arctangent is not valid with x = 0.0

  if(x != 0.0)
  {
    // if x is positive the we use ther returned angle and convert it to degrees
    // it is faster just to mutiply with 100/PI already evaluated
    if(x > 0.0)angle = 57.2958 * atan(y/x);

    // if x is less than 0.0 we have to determine which quardant by looking at Y
    if(x < 0.0) 
    {
      // y below zero subtract 180 from the answer
      if(y < 0.0) angle = 57.2958 * atan(y/x) - 180.0;
      // y > 0.0 add 180
      if(y > 0.0) angle = 57.2958 * atan(y/x) + 180.0;
    }
  }
  

  // print out the results and repeat.
  Serial.print("X = ");
  Serial.print(x) ; 
  Serial.print(" Y = ");
  Serial.print(y); 
  Serial.print(" Z = ");
  Serial.print(z);
  Serial.print(" Ang = ");
  Serial.println(angle);

  // things that would be good to have. check a status byte in the eeprom for calibration
  // if not there do a calibration. If there then read the max's and min's. if not
  // then do a calibration

  // I hope this helps someone out. the MMC5886MC has little information about it.
  
}

void calibrate()
{
  // we have to get the max values and the min values of each axis.
  // in orger to do this we read the x,y,z values for 10000 times while
  // manually rotating the chip in a random patern 360 degrees around the 
  // x, y and z axes. on an uno or nano this takes about 10 to 12 seconds 
  // storage for the data from the chip
  unsigned short xLSB;
  unsigned short xMSB;
  unsigned short yLSB;
  unsigned short yMSB;
  unsigned short zLSB;
  unsigned short zMSB;
  long sx;
  long sy;
  long sz;
  int c = 0;
  static int count = 0;
  while (count < 10000)
  {
    // send a 1 to register 0x8 to read the magnetometer values
    Wire.beginTransmission(MMC_ADDRESS);
    Wire.write(0x08);
    Wire.write(1);
    Wire.endTransmission();
    // we have to continually read the status register and look for bit zero to go TRUE 
    while((c & 1) == 0)
    {
      // write the register 0x7
      Wire.beginTransmission(MMC_ADDRESS);
      Wire.write(7);
      Wire.endTransmission();
      // read 1 byte
      Wire.requestFrom(MMC_ADDRESS, 1);
      while(Wire.available()<1);
      c = Wire.read();
      Wire.endTransmission();
      // let the WHILE evaluate the results once a TRUE condition is detected we continue on
    }
   // Serial.print(c,HEX);
    //Serial.print("  ");  
    // set the read register to 0
    Wire.beginTransmission(MMC_ADDRESS);
    Wire.write(0);
    Wire.endTransmission();
    // request 6 values
    Wire.requestFrom(MMC_ADDRESS, 6);
    // wait until 6 are recieved
    while(Wire.available()<6);
    // read the six values
    // I used shot int to prevent negative number 
    // they all need to be positive
    xLSB = Wire.read();
    xMSB = Wire.read();
    yLSB = Wire.read();
    yMSB = Wire.read();
    zLSB = Wire.read();
    zMSB = Wire.read();
    
    Wire.endTransmission();
    // shifting a byte left 8 spaces multiplys it by 256 to move it to the
    // upper byte position in a word. Adding the lsb gives the true number
    // I used long here to prevent any negatives
    sx = (long)(xMSB << 8) + xLSB;
    sy = (long)(yMSB << 8) + yLSB;
    sz = (long)(zMSB << 8) + zLSB;
    // on the first pass I just capture the initial values
    if(count == 0)
    {
      xMax = xMin = sx;
      yMax = yMin = sy;
      zMax = zMin = sz;
          
    }
    // then I determine if it is a max or a min or neither
    if(xMax < sx)xMax = sx;
    if(xMin > sx)xMin = sx;
    if(yMax < sy)yMax = sy;
    if(yMin > sy)yMin = sy;
    if(zMax < sz)zMax = sz;
    if(zMin > sz)zMin = sz;
    /*Serial.print("X = ");
    Serial.print(sx) ; 
    Serial.print(" Y = ");
    Serial.print(sy); 
    Serial.print(" Z = ");
    Serial.println(sz);*/
    /*delay(100);*/
    // just a little debug so I can see how long to go
    // using the mod operator it limits the output to 10 lines 0 through 9000
    if((count % 1000) == 0 )Serial.println(count);
    count ++; 
  }

  // do a bit of a printout to see the results of the min and max for each axis
  Serial.print("Max x = ");
  Serial.print(xMax) ; 
  Serial.print(" Max y = ");
  Serial.print(yMax); 
  Serial.print(" Max z = ");
  Serial.println(zMax);
  Serial.print("Min x = ");
  Serial.print(xMin) ; 
  Serial.print(" Min y = ");
  Serial.print(yMin); 
  Serial.print(" Min z = ");
  Serial.println(zMin);
    
}


void loop()
{
  readData();
  delay(500);
}
