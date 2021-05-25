
//#define DEBUG   // turn off to get rid of debug messages
#define FIRMWARE_VERSION 0.1
#define SERIAL_BUFFER_SIZE 256

#include <SPI.h>
#include "DebugUtils.h"


// VSPI and HSPI pin definition

#define VSPI_MISO   MISO
#define VSPI_MOSI   MOSI
#define VSPI_SCLK   SCK
#define VSPI_SS     SS

#define HSPI_MISO   12
#define HSPI_MOSI   13
#define HSPI_SCLK   14
#define HSPI_SS     15



// FEATURE FLAGS
boolean USING_DEBUG_READABLE = false;
boolean USING_DEBUG_FEATURE_READ = false;
boolean USING_DEBUG_SERIAL_WRITE = false;
boolean USING_PROCESSING = !USING_DEBUG_READABLE;
boolean USING_FOD = true;

// USER VARIABLES
int FRAME_RATE = 50; int EXPOSURE_TIME_USEC = 1000;
int GAIN = 3;
int FEATURE_SIZE_PER_OBJ = 16;
int TRACKING_OBJ_NUM = 16;
float RESOLUTION_SCALE_FACTOR = 1.0;
boolean USING_FRAME_SUBTRACTION = false;
int USING_DEBUG_IMAGES = 0;

  // USING_DEGUG_IMAGES sets the type of debug image to present
  // 0 = OFF
  // 5 = 16 fixed objects
  // 6 = 4 fixed objects
  // 7 = 2 circling moving objects
  // 11 = 4 fixed 1-pixel boundary objects
 
 // FIXED VARIABLE
/*
FIXED Variables
*/
int spi_speed = 8000000;   // 14Mhz is max
boolean ENABLE = false;
int TOTAL_FEATURE_SIZE = FEATURE_SIZE_PER_OBJ * TRACKING_OBJ_NUM;
static int MOT_NATIVE_X_RESOLUTION = 98;
static int MOT_NATIVE_Y_RESOLUTION = 98;
int MOT_SCALED_X_RESOLUTION = (int)(MOT_NATIVE_X_RESOLUTION * RESOLUTION_SCALE_FACTOR);
int MOT_SCALED_Y_RESOLUTION = (int)(MOT_NATIVE_Y_RESOLUTION * RESOLUTION_SCALE_FACTOR);
byte GAIN_GLOBAL = 15;    // defaults
byte GAIN_GGH = 0;    // defaults
int GUI_OFFSET_X = 50;
int GUI_OFFSET_Y = 50;
int TRANSLATION_OFFSET_X = MOT_SCALED_X_RESOLUTION;
int TRANSLATION_OFFSET_Y = MOT_SCALED_Y_RESOLUTION;
long BAUD_RATE = 115200;
byte FEATURE_BUFFER_0[256];
byte FEATURE_BUFFER_1[256];
byte SETTINGS_BUFFER[16];
uint32_t COUNTER = 0;
float FRAME_RATE_MICROSECONDS_FLOAT = (1.0 / FRAME_RATE)*1000000 / 2 ; // 50% duty cycle for interupt flip-flop
boolean LED_STATE = false;

String DEBUG_FEATURE_STRING_5 = "CC0019000100C8C838003200030C320008005D000000B5B5615A61000006070070005D000C009898455A61061300000026001D000800B7B70314260809081200C40006001700A4A417000D111E0000000D003D001100AFAF02374311110B0C00F7044B002D00C4C40F3C5A19410000002C0122002900E3E3091431252E00000054000B002A00F8F8050512282D00000009005E003C00D1D1015D5F3B3D000000110001004800DCDC02010140500F00F0040060004200E7E7416061424300000058002D005100F2F20528324E5500000058001E005500E9E9051B22505A00000030014E005D00FEFEC93C615A6104250006000C006100FFFF810A0F6161040500";

// pin definition
uint8_t cs_pin = VSPI_SS;
static uint8_t cs_pin_0 = VSPI_SS; 
static uint8_t cs_pin_1 = HSPI_SS;
//uint8_t vsync_pin = 2;
static uint8_t vsync_pin_0 = 2; // ToDO: Vsync pin is optional. If used, change it to any available GPIO in esp32.
static uint8_t vsync_pin_1 = 3;// ToDO: Vsync pin is optional. If used, change it to any available GPIO in esp32.
static int mot_0 = cs_pin_0;
static int mot_1 = cs_pin_1;
//static int fod_pin = 4;

int lf = 10;
int cr = 13;
/**
In ASCII encoding, \n is the Newline character 0x0A (decimal 10), \r is the Carriage Return character 0x0D (decimal 13).

As Jack has said already, the correct sequence is CR-LF, not vice versa.

FTP is probably adding LF characters to your stream if they are placed incorrectly and you are transmitting the file as Text.
**/

// IntervalTimer object
IntervalTimer fps_timer;

SPIClass * vspi = NULL;
SPIClass * hspi = NULL;

void read_settings(){
  if (SETTINGS_BUFFER[0] == "FF")  // Header byte
  {
    /*
       0 = disable
       1 = enable
    */
    ENABLE = SETTINGS_BUFFER[1];

    /*
       Settings buffer 2 = lb, Settings buffer 3 = hb
    */
    FRAME_RATE = SETTINGS_BUFFER[3] << 8 && SETTINGS_BUFFER[2];

    /*
       Settings buffer 4 = lb, Setttings buffer 5 = hb
    */
    EXPOSURE_TIME_USEC = SETTINGS_BUFFER[5] << 8 && SETTINGS_BUFFER[4];

    /*
       Settings buffer 6
    */
    GAIN = SETTINGS_BUFFER[6];

    /*
       Settings buffer 7
    */
    FEATURE_SIZE_PER_OBJ =  SETTINGS_BUFFER[7];

    /*
       Settings buffer 8
    */
    TRACKING_OBJ_NUM = SETTINGS_BUFFER[8];

    /*
       Settings buffer 9 = lb, Setttings buffer 10 = hb
    */
    MOT_SCALED_X_RESOLUTION = SETTINGS_BUFFER[10] << 8 && SETTINGS_BUFFER[9];

    /*
       Settings buffer 11 = lb, Setttings buffer 12 = hb
    */
    MOT_SCALED_Y_RESOLUTION = SETTINGS_BUFFER[12] << 8 && SETTINGS_BUFFER[11];
  }
  else
  {

  }
}

void setup() {
  // Start serial
  if (USING_DEBUG_READABLE)
  {
    Serial.begin(BAUD_RATE);
  }
  else
  {
    Serial.begin(BAUD_RATE);
    //Serial.setTimeout(1500);  Don't think this is needed
    Serial.flush();
  }
  //while (!Serial.dtr()); // waits for Serial to be established

  /*
     SPI
     14Mhz is the max SPI speed for the MOT
     MOT clocks in LSB First
     MOT uses MODE3
  */
  vspi = new SPIClass(VSPI);
  hspi = new SPIClass(HSPI);

  vspi->begin();
  vspi->beginTransaction(SPISettings(spi_speed, LSBFIRST, SPI_MODE3));  // MOT0 supports up to 14Mhz
  hspi->begin();
  hspi->beginTransaction(SPISettings(spi_speed, LSBFIRST, SPI_MODE3));  // MOT1 supports up to 14Mhz

  // pins
  pinMode(cs_pin_0, OUTPUT);
  digitalWrite(cs_pin_0, HIGH);
  pinMode(cs_pin_1, OUTPUT);
  digitalWrite(cs_pin_1, HIGH);
  
  
  // TODO: Please set all the GPIO's used with the IR LEDS here ==> pinMode(gpio_pin_number, OUTPUT); digitalWrite(gpio_pin_number, HIGH);
  if (USING_FOD)
  {

    pinMode(vsync_pin_0, OUTPUT);
    pinMode(vsync_pin_1, OUTPUT);
    digitalWrite(vsync_pin_0, LED_STATE);
    digitalWrite(vsync_pin_1, LED_STATE);
  }
  else
  {
    pinMode(vsync_pin_0, INPUT);
    attachInterrupt(digitalPinToInterrupt(vsync_pin_0), interruptEvent, FALLING);
    pinMode(vsync_pin_1, INPUT);
  }

  // initialize MOT0
  MOT_select(mot_0);
  MOT_initialize();
  checkModel();

  delay(10);
  // initialize MOT1
  MOT_select(mot_1);
  MOT_initialize();
  checkModel();

  fps_timer.begin(counter_interrupt, FRAME_RATE_MICROSECONDS_FLOAT);
}

void loop() {
  // put your main code here, to run repeatedly:

  if (USING_DEBUG_READABLE)
  {
        //Serial.println("I'm alive");
  }
  else
  {
    while(Serial.available()){
      {
        //get char
        char inChar = (char)Serial.read();
        if(inChar == 102){  // 'f'
          send_features_format();
          Serial.flush();
        }
        else if(inChar == 115){ // 's'
          set_settings();
          Serial.flush();
        }
        else if (inChar == lf || inChar == cr){     // carriage return or new line do nothing
          Serial.flush();
        }
        else{
          Serial.print("ERROR");
          Serial.flush();
        }
      }
    }
  }
}

void SerializeInt32(char (&buf)[4], int32_t val)
{
    uint32_t uval = val;
    buf[0] = uval;
    buf[1] = uval >> 8;
    buf[2] = uval >> 16;
    buf[3] = uval >> 24;
}

int32_t ParseInt32(const char (&buf)[4])
{
    // This prevents buf[i] from being promoted to a signed int.
    uint32_t u0 = buf[0], u1 = buf[1], u2 = buf[2], u3 = buf[3];
    uint32_t uval = u0 | (u1 << 8) | (u2 << 16) | (u3 << 24);
    return uval;
}


byte Interface_read(byte addr){
  // Variables
  byte output[3];

  // Set address bytes
  output[0] = (byte)0X80;
  output[1] = addr;
  output[2] = (byte)0x00;

  // CS pin low
  digitalWrite(cs_pin, LOW);

  // SPI transfer
  if (cs_pin == HSPI_SS)
	hspi->transfer(output, 3);
  else
	vspi->transfer(output, 3);  

  // CS pin high
  digitalWrite(cs_pin, HIGH);

  // Return byte 2
  return output[2];
}

int Interface_read(byte* result_buffer, int read_length, byte start_addr){
  int i = 0;
  byte out_stream[read_length + 3]; // 2 for header, 1 for n-1 math
  memset(out_stream,0XFF,read_length + 3); // set all out pulses to FF for debug

  // load the transfer and address bytes
  out_stream[0] = (byte)0X81;
  out_stream[1] = (byte)start_addr;

  // CS pin low
  digitalWrite(cs_pin, LOW);

  // SPI transfer
  // SPI transfer
  if (cs_pin == HSPI_SS)
	hspi->transfer(out_stream, read_length+2);
  else
	vspi->transfer(out_stream, read_length+2);  
  //SPI.transfer(out_stream, read_length+2);

  // CS pin high
  digitalWrite(cs_pin, HIGH); 

  //Prune the first 2 bytes
  for (i = 2; i < read_length+2; i++)
  {
    result_buffer[i - 2] = out_stream[i];
  }

  // return the number of bytes
  return i - 2;
}

void Interface_write(byte addr, byte data){
  // Variables
  byte output[3];

  // Set output buffer to include addr bytes
  output[0] = (byte)0x00;
  output[1] = addr;

  // Set data bytes
  output[2] = data;

  // CS pin low
  //digitalWrite(cs_pin, LOW);

  // SPI transfer
  if (cs_pin == HSPI_SS)
	hspi->transfer(output, 3);
  else
	vspi->transfer(output, 3);  
  //SPI.transfer(output, 3);

  // CS pin high
  //digitalWrite(cs_pin, HIGH);
}
/*
  REGISTER TOOLS
*/
void change_bank(byte bank){
  Interface_write((byte)0xEF, bank);
}

void MOT_apply_command(byte bank){
  change_bank(bank);                  // write APPLY_COMMAND_1
  Interface_write((byte)0X01, (byte)0X01);  // write APPLY_COMMAND_2
}

void sensor_initial_settings(){
  // All according to the data sheet pages 16 and 17
  change_bank((byte)0X00);
  Interface_write((byte)0XDC, (byte)0X00);  // internal systesm control disable
  Interface_write((byte)0XFB, (byte)0X04);  // [2]LEDDAC disable
  //change_bank((byte)0X00);
  Interface_write((byte)0X2F, (byte)0X05);  // sensor ON
  Interface_write((byte)0X30, (byte)0X00);
  Interface_write((byte)0X30, (byte)0X01);  // power control updated
  Interface_write((byte)0X1F, (byte)0X00);  // freerun_irtx_disable

  /*
    GPIO for strobe and vsync out
  */
  /*  Not in initialization code in data sheet
    change_bank((byte)0X00);  //???
    // set g6 as VSYNC out and set G7 as timing of exposure
    Interface_write((byte)0X4C, (byte)0X23);  //???
  */
  MOT_apply_command((byte)0X01);

  change_bank((byte)0X01);
  Interface_write((byte)0X2D, (byte)0X00);  // VFlip

  change_bank((byte)0X0C);
  Interface_write((byte)0X64, (byte)0X00);  // mode setting
  Interface_write((byte)0X65, (byte)0X00);  // mode setting
  Interface_write((byte)0X66, (byte)0X00);  // mode setting
  Interface_write((byte)0X67, (byte)0X00);  // mode setting
  Interface_write((byte)0X68, (byte)0X00);  // mode setting
  Interface_write((byte)0X69, (byte)0X00);  // mode setting

  sensor_FOD_ON_settings(USING_FOD);
  // mode setting, set g6 as other GPIO  was 08
  //Interface_write((byte)0X6A, (byte)0X08);  // controls FOD

  sensor_EXPOSURE_ON_settings(true);
  // mode setting, set g7 as other GPIO  was 08
  //Interface_write((byte)0X6B, (byte)0X08);

  ////Cmd_IOMode_GPIO_08[3:0]
  Interface_write((byte)0X6C, (byte)0X00);  // mode setting

  // mode setting (G13 / LED_SIDE of shared pin) was 08
  //Interface_write((byte)0X71, (byte)0X00);
  Interface_write((byte)0X72, (byte)0X00);  // mode setting
  Interface_write((byte)0X12, (byte)0X00);  // keyscan disable
  Interface_write((byte)0X13, (byte)0X00);  // keyscan disable
  MOT_apply_command((byte)0X00);
}

void sensor_frame_period_settings(int fps){
  // insert some maths to calulate the period
  float frame_time = 1.0 / fps;
  long frame_long = frame_time * 10000000;

  byte *frame_period_byte = (byte *)&frame_long;


  if (USING_DEBUG_READABLE)
  {
    Serial.print("fps = ");
    Serial.print(fps);
    Serial.print(" frame time = ");
    Serial.println(frame_time, 4);
    Serial.print("frame_long = ");
    Serial.print(frame_long);
    Serial.print(" = ");
    Serial.println(frame_long / 10000000, 4);
    Serial.println(frame_period_byte[0], HEX);
    Serial.println(frame_period_byte[1], HEX);
    Serial.println(frame_period_byte[2], HEX);
    Serial.println(frame_period_byte[3], HEX);
  }

  // Write frame periods
  change_bank((byte)0x0C);
  Interface_write((byte)0X07, frame_period_byte[0]);  // frame period lb
  Interface_write((byte)0X08, frame_period_byte[1]);  // frame period hb
  Interface_write((byte)0X09, frame_period_byte[2]);  // frame period hhb
}

void sensor_gain_settings(byte global, byte ggh){
  // write registers
  change_bank((byte)0X0C);
  Interface_write((byte)0X0B, (byte)global);  // write B_global
  Interface_write((byte)0X0C, (byte)ggh);  // write B_ggh
  MOT_apply_command((byte)0X01);
  MOT_apply_command((byte)0X00);
}

void sensor_exposure_settings(int exposure_uSec){
  // calculate maximum allowable frame rate in uSec
  int frame_rate_uSec = (int)((1 / (float)FRAME_RATE) * 1000000) - 2700;
  //println("input exposure time in uSec = " + exposure_uSec);
  int exposure_time = 0;

  // maths for exposure
  if (exposure_uSec > frame_rate_uSec)
  {
    Serial.print("ERROR: Exposure time is set too high for the frame rate");
    Serial.println("- reduced exposure time");
    exposure_uSec = frame_rate_uSec;
    Serial.print("Maximum allowable exposure length in uSec = ");
    Serial.print(frame_rate_uSec);
    Serial.println(" uSec");
  }

  exposure_time = (int)(exposure_uSec / .2);
  if (USING_DEBUG_READABLE)
  {
    Serial.println("output int = " + exposure_time);
    Serial.print("compensated output = ");
    Serial.print(exposure_time * .2);
    Serial.println(" uSec");
  }
  // write sensor exposure
  // default is exposure is 0X2000
  change_bank((byte)0X0C);
  Interface_write((byte)0X0F, lowByte(exposure_time));  // write B_expo_lb
  Interface_write((byte)0X10, highByte(exposure_time));  // write B_expo_hb
  MOT_apply_command((byte)0X01);
}

void sensor_dsp_settings(){
  change_bank((byte)0X0C);
  Interface_write((byte)0X46, (byte)0X03);  // area min threshold
  Interface_write((byte)0X47, (byte)0X97);  // brightness threshold

  change_bank((byte)0X00);
  Interface_write((byte)0X0B, (byte)0X85);  // max area threshold
  Interface_write((byte)0X0C, (byte)0X25);  // max area threshold
  Interface_write((byte)0X0F, (byte)0X0A);  // noise threshold
  Interface_write((byte)0X2B, (byte)0X01);  // unknown
  MOT_apply_command((byte)0X00);
}

void sensor_resolution_settings(int x_resolution, int y_resolution){
  change_bank((byte)0X0C);
  Interface_write((byte)0X60, lowByte(x_resolution));  // x resolution lb
  Interface_write((byte)0X61, highByte(x_resolution));  // x resolution hb = 2940
  Interface_write((byte)0X62, lowByte(y_resolution));  // y resolution lb
  Interface_write((byte)0X63, highByte(x_resolution));  // y resolution hb = 2940

  /*  // defaults
    Interface_write((byte)0X60, (byte)0X7C);  // x resolution lb
    Interface_write((byte)0X61, (byte)0X0B);  // x resolution hb = 2940
    Interface_write((byte)0X62, (byte)0X7C);  // y resolution lb
    Interface_write((byte)0X63, (byte)0X0B);  // y resolution hb = 2940
  */
  MOT_apply_command((byte)0X01);
}

void sensor_LED_GPIO_ON_settings(boolean led_on, boolean led_frame_subtraction){
  change_bank((byte)0X0C);
  // mode setting (G13 / LED_SIDE of shared pin) was 08
  //Cmd_IOMode_GPIO_013[3:0]
  Interface_write((byte)0X71, ((byte)(led_on ? 0X08 : 0X00 )));  // was 08
  change_bank((byte)0X00);
  // if led_frame_subtraction is on, it will skip every other frame, otherwise will output the exposure window
  // need to leave this in frame exposure mode to ensure that the subtraction frame is indicated somewhere
  Interface_write((byte)0X4F, ((byte)(led_frame_subtraction ? 0X5C : 0X2C)));  // was in original settings list (default setting DC(off))
  MOT_apply_command((byte)0X00);
}

void sensor_debug_settings(int mode){
  change_bank((byte)0X01);
  Interface_write((byte)0X2B, (byte)mode);
}

void sensor_FOD_ON_settings(boolean FOD_on){
  change_bank((byte)0X0C);
  //Cmd_IOMode_GPIO_06[3:0]
  Interface_write((byte)0X6A, (byte)(FOD_on ? 0X07 : 0X00 ));  // FOD on
  MOT_apply_command((byte)0X00);
}

void sensor_EXPOSURE_ON_settings(boolean exposure_on){
  Interface_write((byte)0X6B, (byte)(exposure_on ? 0X08 : 0X00 ));  // Exposure GPIO ON
}

void sensor_frame_subraction_settings(boolean frame_subtraction_on){
  change_bank((byte)0X00);
  // frame subraction enable
  Interface_write((byte)0X28, (byte)(frame_subtraction_on ? 0X01 : 0X00));
  MOT_apply_command((byte)0X00);
}

void get_features_format_1(int buffer_number){

	int index = 0;
  // read data from MOT
  int count_read = 0;

  if(buffer_number == 0){
    MOT_select(cs_pin_0); 
    change_bank((byte)0X05);    // read from bank 5
    count_read = Interface_read(FEATURE_BUFFER_0, 256, (byte)0X00);
  }

	else if (buffer_number == 1){
    MOT_select(cs_pin_1); 
    change_bank((byte)0X05);    // read from bank 5
		count_read = Interface_read(FEATURE_BUFFER_1, 256, (byte)0X00);
	}
	else{
		// nothing
	}
  	
  if (USING_DEBUG_READABLE){

  		for (int i=0; i < 256; i++)
  		{
  			Serial.print(buffer_number == 0 ? FEATURE_BUFFER_0[i] : FEATURE_BUFFER_1[i]);
  		}
  		Serial.println("");
  		
    	Serial.print( "read ");
    	Serial.print(count_read);
    	Serial.println(" bytes");

    	Serial.print( "sizeof says ");
    	Serial.print(sizeof(buffer_number == 0 ? FEATURE_BUFFER_0 : FEATURE_BUFFER_1));
    	Serial.print( " for Feature buffer ");
    	Serial.println(buffer_number);
  		
  	}
}

void send_features_format(){
	// send counter and millis counter frames
  char counter_string[4];
  SerializeInt32(counter_string,COUNTER);
  for(int i = 0 ; i< sizeof(counter_string);i++){
    Serial.write(counter_string[i]);
  }
  /*
  Serial.print(",");
  Serial.write(millis());
  Serial.write(cr);
  */
  //Serial.send_now();  // not needed
  
  // debug string to char[]
  char test_char_str[256];
  if(USING_DEBUG_SERIAL_WRITE)
  {
    // debug string to char[]
    DEBUG_FEATURE_STRING_5.toCharArray(test_char_str,256);
    // output after Serial.write is in integer format
    // 20402501020020056050031250080930001811819790970067011209301201521526990976190003802908018318332038898180196060230164164230131730000130610170175175255671717111202474750450196196156090256500044134041022722792049374600084011042024824855184045000909406002092091939559610001701072022022021164801502404096066023123165969766670008804508102422425405078850008803008502332335273480900004817809302542542016097909743706012097025525512910159797450
  }
    
  // send buffer 0
  for (int i=0; i < 256; i++){
  	// Serial.write(FEATURE_BUFFER_0[i]);
    Serial.write(USING_DEBUG_SERIAL_WRITE ? test_char_str[i] : FEATURE_BUFFER_0[i]);
  	}
  //Serial.write(cr);
	
  // send buffer 1
	for (int i=0; i < 256; i++){
  	// Serial.write(FEATURE_BUFFER_1[i]);
    Serial.write(USING_DEBUG_SERIAL_WRITE ? test_char_str[i] : FEATURE_BUFFER_1[i]);
  	}
	//Serial.write(cr);
}

void MOT_power_down(){
  change_bank((byte)0X00);
  Interface_write((byte)0X2F, (byte)0X00);
  Interface_write((byte)0X30, (byte)0X00);
  Interface_write((byte)0X31, (byte)0X01);
  MOT_apply_command((byte)0X00);
}

void MOT_reset(){
  change_bank((byte)0X00);
  Interface_write((byte)0X64, (byte)0X00);
}

int getModel(){
  // Change bank to 0
  change_bank((byte)0X00);
  byte model_lb = Interface_read((byte)0X02);
  byte model_hb = Interface_read((byte)0X03);
  int model = model_hb << 8;
  model += model_lb;
  return model;
}

void checkModel(){
  int model = getModel();
  if (USING_DEBUG_READABLE)
  {
    Serial.print("Checking Model - ");
    Serial.print("model number = ");
    Serial.print(model);
    if (model == 28709)
    {
      Serial.print(" - CHECK PASSED");
    }
    else
    {
      Serial.print(" - CHECK FAILED");
    }
  }
}

void MOT_initialize(){
  /*
    Initialize settings
  */
  sensor_initial_settings();

  /*
    Set frame period
  */
  //Interface_write((byte)0XEF, (byte)0X0C);  // switch to bank 12
  //Interface_write((byte)0X07, (byte)0X74);  // frame period
  //Interface_write((byte)0X08, (byte)0XC2);  // frame period
  //Interface_write((byte)0X09, (byte)0X00);  // frame period
  sensor_frame_period_settings(FRAME_RATE);

  /*
    Set gain
  */
  //change_bank((byte)0X0C);
  //Interface_write((byte)0X0B, (byte)0X10);  // write B_global
  //Interface_write((byte)0X0C, (byte)0X00);  // write B_ggh
  sensor_gain_settings(GAIN_GLOBAL, GAIN_GGH);
  // default gain would be setting 17 (gain of 2)

  /*
    Set exposure    default is 0x2000 = 8192 * 200nS = 1.6384mS exposure time
  */
  //Interface_write((byte)0X0F, (byte)0X00);  // write B_expo_lb
  //Interface_write((byte)0X10, (byte)0X20);  // write B_expo_hb
  sensor_exposure_settings(EXPOSURE_TIME_USEC);

  /*
    Set DSP settings
  */
  sensor_dsp_settings();
  /*
    change_bank((byte)0X0C);
    Interface_write((byte)0X46, (byte)0X03);  // oalb 0
    Interface_write((byte)0X47, (byte)0X97);  // yth 151
    MOT_apply_command((byte)0X01); */

  /*  // Set resolution scale
    Interface_write((byte)0X60, (byte)0X7C);  // x resolution
    Interface_write((byte)0X61, (byte)0X0B);  // x resolution = 2940
    Interface_write((byte)0X62, (byte)0X7C);  // y resolution
    Interface_write((byte)0X63, (byte)0X0B);  // y resolution = 2940
  */
  sensor_resolution_settings(MOT_SCALED_X_RESOLUTION, MOT_SCALED_Y_RESOLUTION);
  // default is (2940,2940)

  /*  In dsp settings
    change_bank((byte)0X00);
    Interface_write((byte)0X0B, (byte)0X85);  // oahb
    Interface_write((byte)0X0C, (byte)0X25);  // oahb = 9605
    Interface_write((byte)0X0F, (byte)0X0A);  // nth = 10
    Interface_write((byte)0X28, (byte)0X00);  // frame subraction enable
    MOT_apply_command((byte)0X01);
  */
 
  // turn on frame subtraction
  sensor_frame_subraction_settings(USING_FRAME_SUBTRACTION);
  sensor_LED_GPIO_ON_settings(true, true);	// LED on and skipping ever other frame

  // turn off debug picture mode
  sensor_debug_settings(USING_DEBUG_IMAGES);
}

void interruptEvent(){
  //  if (digitalRead(vsync_pin_0) == true)
  //  {
  //    send_features_format();
  //  }
  //  else
  //  {
  MOT_select(mot_0);
  get_features_format_1(0);
  MOT_select(mot_1);
  get_features_format_1(1);
  //  }
}

void MOT_select(int mot_number){
  cs_pin = mot_number;
}

void counter_interrupt(){
  if(LED_STATE)
  {
    //send features to serial
    //send_features_format();
  }
  else
  {
    //get features from both sensors
    get_features_format_1(0);
    get_features_format_1(1);
    // increment counter
    COUNTER++;  
  }
  LED_STATE = !LED_STATE;
  digitalWrite(vsync_pin_0, LED_STATE);
  digitalWrite(vsync_pin_1, LED_STATE);
}

void set_settings(){

}


/*
void serialEvent(){
  while(Serial.available()){
    {
      get char
      char inChar = (char)Serial.read();
      if(inChar == 102){  // 'f'
        send_features_format();
      }
      else if(inChar == 115){ // 's'
        set_settings();
      }
      else{
        Serial.print("ERROR");
      }
    }
  }
}
*/
