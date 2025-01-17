// Xiaoxiang Smart BMS Display
// For Arduino Pro mini / Arduino Nano
// Code is based on a heavily modified version of: https://github.com/bres55/Smart-BMS-arduino-Reader

// This program reads data from the BMS via a secondary software serial port.
// It then prints it out to the computer via the arduino's main serial port.
// Additionally, it prints the important data to an OLED display.
// Note that the Display code only works for 4S batteries and will need to be heavily modified for other configurations.
// It shouldn't be too hard to modify this code to work with other display types as well,
// since the display code is mostly contained within its own subroutine.

#include <SoftwareSerial.h> // Second serial port for connecting to the BMS
#include <Wire.h>



#define specifiction_info 0 //packed specification number and scaling information
#define serial_number 0

byte register_to_call = 0;
int16_t current = 0; //charge/discharge rate in mA increments - positive for charge, negative for discharge, 0 to 32,767 mA for charge or 0 to -32,768 mA for discharge
uint16_t remaining_capacity = 0;  //remaining charge in mAh or 10mWh
uint16_t temperature = 0; //cell temperature in tenth degree Kelvin increments, 0 to +6553.5°K
uint16_t overall_voltage = 0; //battery terminal voltage in milli-volts, 0 to 65,535 mV
uint16_t cell_voltage[13] = {0, 0, 0, 0, 0, 0 ,0 ,0 ,0 ,0 ,0 ,0 ,0}; //battery cell voltage in milli-volts, 0 to 65,535 mV
uint16_t full_charge_capacity = 20000; //estimated full charge capacity in mAh or 10mWh
uint16_t cycle_count = 0; //number of times the battery has been charged, 0 to 65535, 65535 indicates that the battery has been charged exactly that many times or more




SoftwareSerial MySoftSerial(5, 6); // RX, TX
#define MySerial MySoftSerial

// Variables
String inString = ""; // string to hold input
int incomingByte, BalanceCode, Length, highbyte, lowbyte;
byte Mosfet_control, mosfetnow, BatteryConfigH, BatteryConfigL, bcl, bcln, bch, Checksum, switche;
uint8_t BYTE1, BYTE2, BYTE3, BYTE4, BYTE5, BYTE6, BYTE7, BYTE8, BYTE9, BYTE10;
uint8_t inInts[40], data[9];   // an array to hold incoming data, not seen any longer than 34 bytes, or 9
uint16_t a16bitvar;
float eresultf; //Cellv1, Cellv2, Cellv3, Cellv4, Cellv5, Cellv6, Cellv7, Cellv8,

// Global battery stat variables (For printing to displays)
float CellMin = 5; // Default value > max possible cell votlage
float CellMax = 0;
float Cellavg = 0;
float Celldiff = 0;
float myCellVoltages[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int balancerStates[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int myNumCells = 0;
float PackVoltagef = 0;
float PackCurrentf = 0;
float RemainCapacityf = 0;
int RSOC = 0;
float Temp_probe_1f = 0;
float Temp_probe_2f = 0;
bool chargeFet = 0;
bool dischargeFet = 0;
bool cellOver = 0;
bool cellUnder = 0;
bool packOver = 0;
bool PackUnder = 0;
bool chargeOverTemp = 0;
bool chargeUnderTemp = 0;
bool dischargeOverTemp = 0;
bool dischargeUnderTemp = 0;
bool chargeOvercurrent = 0;
bool dischargeOvercurrent = 0;
bool shortCircuit = 0;
bool AFEerror = 0;


void requestEvent(void) {
  switch (register_to_call) {
    case 0x0A: //current request
      Wire.write(lowByte(current));
      Wire.write(highByte(current));
      break;
    case 0x0F: //remaining capacity request
      Wire.write(lowByte(remaining_capacity));
      Wire.write(highByte(remaining_capacity));
      break;
    case 0x08: //temperature request
      Wire.write(lowByte(temperature));
      Wire.write(highByte(temperature));
      break;
    case 0x09: //overall voltage request
      Wire.write(lowByte(overall_voltage));
      Wire.write(highByte(overall_voltage));
      break;
    case 0x3f: //cell 1 voltage request
      Wire.write(lowByte(cell_voltage[0]));
      Wire.write(highByte(cell_voltage[0]));
      break;
    case 0x3e: //cell 2 voltage request
      Wire.write(lowByte(cell_voltage[1]));
      Wire.write(highByte(cell_voltage[1]));
      break;
    case 0x3d: //cell 3 voltage request
      Wire.write(lowByte(cell_voltage[2]));
      Wire.write(highByte(cell_voltage[2]));
      break;
    case 0x3c: //cell 4 voltage request
      Wire.write(lowByte(cell_voltage[3]));
      Wire.write(highByte(cell_voltage[3]));
      break;
    case 0x3b: //cell 5 voltage request
      Wire.write(lowByte(cell_voltage[4]));
      Wire.write(highByte(cell_voltage[4]));
      break;
    case 0x3a: //cell 6 voltage request
      Wire.write(lowByte(cell_voltage[5]));
      Wire.write(highByte(cell_voltage[5]));
      break;
    case 0x1a: //specification_Info
      Wire.write(lowByte(specifiction_info));
      Wire.write(highByte(specifiction_info));
      break;
    case 0x10: //full charge capacity
      Wire.write(lowByte(full_charge_capacity));
      Wire.write(highByte(full_charge_capacity));
      break;
    case 0x1c: //serial number
      Wire.write(lowByte(serial_number));
      Wire.write(highByte(serial_number));
      break;
    case 0x17: //cycle count
      Wire.write(lowByte(cycle_count));
      Wire.write(highByte(cycle_count));
      break;

  }

}

void recieveEvent(int Anzahl) {
  while (Wire.available()) {
    register_to_call = Wire.read();
  }

}



void setup() {
  MySerial.begin(9600);
  Serial.begin(115200);
  Wire.begin(0x0B);
  Wire.onReceive(recieveEvent);
  Wire.onRequest(requestEvent);

}

void loop() {
  takeMeasurements();
  updateMeasurements();
  delay(100);
}

// SUB ROUTINES ////////////////////////////////////////////////////////////////////////////////

void updateMeasurements(void) {
current = PackCurrentf ;
overall_voltage = PackVoltagef ;
temperature = Temp_probe_1f ;
cell_voltage[0] = myCellVoltages[0];
cell_voltage[1] = myCellVoltages[0];
cell_voltage[2] = myCellVoltages[0];
cell_voltage[3] = myCellVoltages[0];
cell_voltage[4] = myCellVoltages[0];
cell_voltage[5] = myCellVoltages[0];
cell_voltage[6] = myCellVoltages[0];
cell_voltage[7] = myCellVoltages[0];
cell_voltage[8] = myCellVoltages[0];
cell_voltage[9] = myCellVoltages[0];
cell_voltage[10] = myCellVoltages[0];
cell_voltage[11] = myCellVoltages[0];
cell_voltage[12] = myCellVoltages[0];
  
}


void takeMeasurements(void) {
  Serial.println(".");
  Serial.println(".");
  Serial.println(".");
  write_request_start();// Found this helps timing issue, by saying hello, hello.
  write_request_end() ; // Or maybe it flushes out any rogue data.
  write_request_start();// Any way it works,
  write_request_end() ; // And accomodates long delays if you want them at the end

  // CELLS VOLTAGE 04 /////////////////////////////////////////////////////////////////////////////
  call_get_cells_v();      // requests cells voltage
  get_bms_feedback();     // returns with up to date, inString= chars, inInts[]= numbers, chksum in last 2 bytes
  //                       Length (length of data string)
  //  got cell voltages, bytes 0 and 1, its 16 bit, high and low
  //  go through and print them
  // Length = Length - 2;
  // print headings
  for (int i = 2; i < (Length + 1); i = i + 2) {
    Serial.print (" Cell ");
    Serial.print (i / 2);
    Serial.print("  ");
  }
  Serial.print (" CellMax "); // CellMax heading
  Serial.print("  ");
  Serial.print (" CellMin "); // CellMin heading
  Serial.print("  ");
  Serial.print (" Diff "); // diference heading
  Serial.print("  ");
  Serial.print ("  Avg "); // Average heading
  Serial.print("  ");
  myNumCells = Length / 2;
  Serial.print (" NumCells:");
  Serial.print (myNumCells);
  Serial.println ("");
  // and the values
  Cellavg = 0;
  CellMax = 0;
  CellMin = 5;
  for (int i = 0; i < Length; i = i + 2) {
    highbyte = (inInts[i]);
    lowbyte = (inInts[i + 1]);
    uint16_t Cellnow = two_ints_into16(highbyte, lowbyte);
    float Cellnowf = Cellnow / 1000.0f; // convert to float
    myCellVoltages[i / 2] = Cellnowf; // Update value in array
    Cellavg = Cellavg + Cellnowf;
    if (Cellnowf > CellMax) {   // get high and low
      CellMax = Cellnowf;
    }
    if (Cellnowf < CellMin) {
      CellMin = Cellnowf;
    }
    Serial.print(" ");
    Serial.print(Cellnowf, 3); // 3 decimal places
    Serial.print("   ");
  }
  Serial.print(" ");
  Serial.print(CellMax, 3); // 3 decimal places
  Serial.print("   ");
  Serial.print("   ");
  Serial.print(CellMin, 3); // 3 decimal places
  Serial.print("   ");
  Celldiff = CellMax - CellMin; // difference between highest and lowest
  Serial.print("   ");
  Serial.print(Celldiff, 3); // 3 decimal places
  Serial.print("   ");
  Cellavg = Cellavg / (Length / 2); // Average of Cells
  Serial.print(" ");
  Serial.print(Cellavg, 3); // 3 decimal places
  Serial.print("   ");


  // USING BASIC INFO 03 get //////////////////////////////////////////////////////////////////////
  //  CELL BALANCE... info
  call_Basic_info();      // requests basic info.
  get_bms_feedback();   // get that data, used to get BALANCE STATE byte 17 less 4, decimal=byte 13
  //  Serial.print(" BC= ");
  BalanceCode = inInts[13]; //  the 13th byte
  BalanceCode = Bit_Reverse( BalanceCode ) ; // reverse the bits, so they are in same order as cells
  //  Serial.print(BalanceCode, BIN); // works, but, loses leading zeros and get confusing on screen
  print_binary(BalanceCode, 8);// print balance state as binary, cell 1 on the right, cell 8 on left
  //                                    Reversed this. 1 on left, 8 on right
  Serial.print ("  Balancer States ");
  Serial.println(" ");

  // PACK VOLTAGE,, bytes 0 and 1, its 16 bit, high and low
  highbyte = (inInts[0]); // bytes 0 and 1
  lowbyte = (inInts[1]);
  uint16_t PackVoltage = two_ints_into16(highbyte, lowbyte);
  PackVoltagef = PackVoltage / 100.0f; // convert to float and leave at 2 dec places
  Serial.print("Pack Voltage = ");
  Serial.print(PackVoltagef);

  // CURRENT
  highbyte = (inInts[2]); // bytes 2 and 3
  lowbyte = (inInts[3]);
  int PackCurrent = two_ints_into16(highbyte, lowbyte);
  // uint16_t PackCurrent = two_ints_into16(highbyte, lowbyte);
  PackCurrentf = PackCurrent / 100.0f; // convert to float and leave at 2 dec places
  Serial.print("   Current = ");
  Serial.print(PackCurrentf);

  //REMAINING CAPACITY
  highbyte = (inInts[4]);
  lowbyte = (inInts[5]);
  uint16_t RemainCapacity = two_ints_into16(highbyte, lowbyte);
  RemainCapacityf = RemainCapacity / 100.0f; // convert to float and leave at 2 dec places
  Serial.print("   Remaining Capacity = ");
  Serial.print(RemainCapacityf);
  Serial.print("Ah");

  //RSOC
  RSOC = (inInts[19]);
  Serial.print("   RSOC = ");
  Serial.print(RSOC);
  Serial.print("%");

  //Temp probe 1
  highbyte = (inInts[23]);
  lowbyte = (inInts[24]);
  float Temp_probe_1 = two_ints_into16(highbyte, lowbyte);
  Temp_probe_1f = (Temp_probe_1 - 2731) / 10.00f; // convert to float and leave at 2 dec places
  Serial.println("");
  Serial.print("Temp probe 1 = ");
  Serial.print(Temp_probe_1f);
  Serial.print(" ");

  //Temp probe 2
  highbyte = (inInts[25]);
  lowbyte = (inInts[26]);
  float Temp_probe_2 = two_ints_into16(highbyte, lowbyte);
  Temp_probe_2f = (Temp_probe_2 - 2731) / 10.00f; // convert to float and leave at 2 dec places
  Serial.print("   Temp probe 2 = ");
  Serial.print(Temp_probe_2f);
  Serial.println(" ");

  // Mosfets
  chargeFet = inInts[20] & 1; //bit0
  dischargeFet = (inInts[20] >> 1) & 1; //bit1
  Serial.print(F("Mosfet Charge = "));
  Serial.print(chargeFet);
  Serial.print(F("  Mosfet DisCharge = "));
  Serial.println(dischargeFet);

  // Pack Protection states
  cellOver = (inInts[17] >> 0) & 1;
  cellUnder = (inInts[17] >> 1) & 1;
  packOver = (inInts[17] >> 2) & 1;
  PackUnder = (inInts[17] >> 3) & 1;
  chargeOverTemp = (inInts[17] >> 4) & 1;
  chargeUnderTemp = (inInts[17] >> 5) & 1;
  dischargeOverTemp = (inInts[17] >> 6) & 1;
  dischargeUnderTemp = (inInts[17] >> 7) & 1;
  chargeOvercurrent = (inInts[16] >> 0) & 1;
  dischargeOvercurrent = (inInts[16] >> 1) & 1;
  shortCircuit = (inInts[16] >> 2) & 1;
  AFEerror = (inInts[16] >> 3) & 1;

  Serial.print("Protection Status: ");
  Serial.print(cellOver);
  Serial.print(cellUnder);
  Serial.print(packOver);
  Serial.print(PackUnder);
  Serial.print(chargeOverTemp);
  Serial.print(chargeUnderTemp);
  Serial.print(dischargeOverTemp);
  Serial.print(dischargeUnderTemp);
  Serial.print(chargeOvercurrent);
  Serial.print(dischargeOvercurrent);
  Serial.print(shortCircuit);
  Serial.print(AFEerror);
  Serial.println("");

  inString = "";
  Length = 0;
}

//------------------------------------------------------------------------------------------
// Do not edit anything below this line
//------------------------------------------------------------------------------------------
//  uint16_t PackCurrent = two_ints_into16(highbyte, lowbyte);
uint16_t two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
  a16bitvar = (highbyte);
  a16bitvar <<= 8; //Left shift 8 bits,
  a16bitvar = (a16bitvar | lowbyte); //OR operation, merge the two
  return a16bitvar;
}
// ----------------------------------------------------------------------------------------------------
void call_Basic_info()
// total voltage, current, Residual capacity, Balanced state, MOSFET control status
{
  flush(); // flush first

  //  DD  A5 03 00  FF  FD  77
  // 221 165  3  0 255 253 119
  uint8_t data[7] = {221, 165, 3, 0, 255, 253, 119};
  MySerial.write(data, 7);
}
//--------------------------------------------------------------------------
void call_get_cells_v()
{
  flush(); // flush first

  // DD  A5  4 0 FF  FC  77
  // 221 165 4 0 255 252 119
  uint8_t data[7] = {221, 165, 4, 0, 255, 252, 119};
  MySerial.write(data, 7);
}
//--------------------------------------------------------------------------
void call_Hardware_info()
{
  flush(); // flush first

  //  DD  A5 05 00  FF  FB  77
  // 221 165  5  0 255 251 119
  uint8_t data[7] = {221, 165, 5, 0, 255, 251, 119};
  // uint8_t data[7] = {DD, A5, 05, 00, FF, FB, 77};
  MySerial.write(data, 7);
}
//--------------------------------------------------------------------------
void write_request_start()
{
  flush(); // flush first

  //   DD 5A 00  02 56  78  FF 30   77
  uint8_t data[9] = {221, 90, 0, 2, 86, 120, 255, 48, 119};
  MySerial.write(data, 9);
}
//----------------------------------------------------------------------------
void write_request_end()
{
  flush(); // flush first

  //   DD 5A 01  02 00 00   FF  FD 77
  uint8_t data[9] = {221, 90, 1, 2, 0, 0, 255, 253, 119};
  MySerial.write(data, 9);
}
//-------------------------------------------------------------------------
void eprom_read()   //BAR CODE
{
  flush(); // flush first
  //delay(5);
  // SENT CODE depends on WHAT IS REQD???
  //   DD  A5  A2 0  FF 5E  77...BAR CODE
  //  221 165 162 0 255 94 119
  // uint8_t data[7] = {221, 165, 162, 0, 255, 94, 119};
  uint8_t data[7] = {221, 165, 32, 0, 255, 224, 119};
  MySerial.write(data, 7);
}

//-------------------------------------------------------------------------
void eprom_end() // no need at mo
{
  flush(); // flush first
  // delay(5);
  //DD  A5  AA  0 FF  56  77
  //221 165 170 0 255 86  119
  // from eprom read
  uint8_t data[7] = {221, 165, 170, 0, 255, 86, 119};
  MySerial.write(data, 7);
}
//------------------------------------------------------------------------------
void flush()
{ // FLUSH
  delay(100); // give it a mo to settle, seems to miss occasionally without this
  while (MySerial.available() > 0)
  { MySerial.read();
  }
  delay(50); // give it a mo to settle, seems to miss occasionally without this
}
//--------------------------------------------------------------------------
void get_bms_feedback()  // returns with up to date, inString= chars, inInts= numbers, chksum in last 2 bytes
//                          Length
//                          Data only, exclude first 3 bytes
{
  inString = ""; // clear instring for new incoming
  delay(100); // give it a mo to settle, seems to miss occasionally without this
  if (MySerial.available() > 0) {
    {
      for (int i = 0; i < 4; i++)               // just get first 4 bytes
      {
        incomingByte = MySerial.read();
        if (i == 3)
        { // could look at 3rd byte, it's the ok signal
          Length = (incomingByte); // The fourth byte holds the length of data, excluding last 3 bytes checksum etc
          // Serial.print(" inc ");
          //Serial.print(incomingByte);
        }
        if (Length == 0) {
          Length = 1; // in some responses, length=0, dont want that, so, make Length=1
        }
      }
      //  Length = Length + 2; // want to get the checksum too, for writing back, saves calculating it later
      for (int i = 0; i < Length + 2; i++) { // get the checksum in last two bytes, just in case need later
        incomingByte = MySerial.read(); // get the rest of the data, how long it might be.
        inString += (char)incomingByte; // convert the incoming byte to a char and add it to the string
        inInts[i] = incomingByte;       // save incoming byte to array as int
      }
    }
  }
}
//-----------------------------------------------------------------------------------------------------
void print_binary(int v, int num_places) // prints integer in binary format, nibbles, with leading zeros
// altered a bit, but got from here,  https://phanderson.com/arduino/arduino_display.html
{
  Serial.println("");
  Serial.print("  ");
  int mask = 0, n;
  for (n = 1; n <= num_places; n++)
  {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask;  // truncate v to specified number of places
  int cellNum = 0;
  while (num_places)
  {
    if (v & (0x0001 << num_places - 1))
    {
      Serial.print("1        ");
      balancerStates[cellNum] = 1;
    }
    else
    {
      Serial.print("0        ");
      balancerStates[cellNum] = 0;
    }
    --num_places;
    if (((num_places % 4) == 0) && (num_places != 0))
    {
      Serial.print("");
    }
    cellNum++;
  }
}
//-----------------------------------------------------------------------------------------------------
byte Bit_Reverse( byte x )
// http://www.nrtm.org/index.php/2013/07/25/reverse-bits-in-a-byte/
{
  //          01010101  |         10101010
  x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
  //          00110011  |         11001100
  x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
  //          00001111  |         11110000
  x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
  return x;
}
