/*
  Cleaned up the code for easier text modifications
  Also, this version can toggle some PINs on the arduino. I'm using it to control
  RGB lights in the foot dwell :-)

  Todo (someone): if possible, enable scrolling text. This should be possible since the text messages is larger than the screen. 
  I guess scrolling can be turned on by
  1) editing some bits in the init messages
  2) editing some bits in the text headers
  3) some specific character combination in the text itself

  By Thomas Landahl, 2017-08-20
  Comm sniffing and some code contribution by Vincent Gijsen. Thanks a LOT!

*/

#define INT_NUM (byte)0         //Interrupt number (0/1 on ATMega 328P)
#define MELBUS_CLOCKBIT (byte)2 //Pin D2  - CLK
#define MELBUS_DATA (byte)3     //Pin D3  - Data
#define MELBUS_BUSY (byte)4     //Pin D4  - Busy

const byte prevPin = 8;
const byte nextPin = 9;
const byte upPin = 10;    //volume up
const byte downPin = 11;  //volume down
const byte playPin = 12;
const byte LEDLEFT = 14;
const byte LEDRIGHT = 15;
const byte LEDB = 16;
const byte LEDG = 17;
const byte LEDR = 18;
const byte LEDMISC1 = 19;
const byte LEDMISC2 = 20;

//volatile variables used inside and outside of ISP
volatile byte melbus_ReceivedByte = 0;
volatile byte melbus_Bitposition = 7;
volatile bool byteIsRead = false;


byte byteToSend = 0;  //global to avoid unnecessary overhead
bool Connected = false;


#define RESPONSE_ID 0xC5  //ID while responding to init requests (which will use base_id)
#define BASE_ID 0xC0      //ID when getting commands from HU                  ///SATNAV ID????; my comment
#define MASTER_ID 0xC7


byte textHeader[] = {0xFC, 0xC6, 0x73, 0x01};
byte textRow = 2;
byte customText[36] = "Visual approach";
//HU asks for line 3 and 4 below at startup. They can be overwritten by customText if you change textRow to 3 or 4
byte textLine[4][36] = {
  {'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'o', 'n', 'e', ' ', 't', 'e', 'x', 't'},
  {'A', 'n', 'o', 't', 'h', 'e', 'r', ' ', 't', 'e', 'x', 't', 'l', 'i', 'n', 'e'},
  {'T', 'h', 'i', 'r', 'd', ' ', 'l', 'i', 'n', 'e'},
  {'F', 'o', 'u', 'r', 't', 'h', ' ', 'l', 'i', 'n', 'e'}
};

const byte C1_Init_1[9] = {
  0x10, 0x00, 0xc3, 0x01,
  0x00, 0x81, 0x01, 0xff,
  0x00
};
const byte SO_C1_Init_1 = 9;

const byte C1_Init_2[11] = {
  (0x10), (0x01), (0x81),
  ('V'), ('o'), ('l'),
  ('v'), ('o'), ('!'),
  (' '), (' ')
};
const byte SO_C1_Init_2 = 11;


const byte C2_Init_1[] = {
  0x10, 0x01, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff
};
const byte SO_C2_Init_1 = 19;


const byte C3_Init_0[30] = {
  0x10, 0x00, 0xfe, 0xff,
  0xff, 0xdf, 0x3f, 0x29,
  0x2c, 0xf0, 0xde, 0x2f,
  0x61, 0xf4, 0xf4, 0xdf,
  0xdd, 0xbf, 0xff, 0xbe,
  0xff, 0xff, 0x03, 0x00,
  0xe0, 0x05, 0x40, 0x00,
  0x00, 0x00
} ;
const byte SO_C3_Init_0 = 30;

const byte C3_Init_1[30] = {
  0x10, 0x01, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff
};
const byte SO_C3_Init_1 = 30;

const byte C3_Init_2[30] = {
  0x10, 0x02, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff
};
const byte SO_C3_Init_2 = 30;




//Defining the commands. First byte is the length of the command.
#define MRB_1 {3, 0x00, 0x1C, 0xEC}            //Master Request Broadcast version 1
#define MI {3, 0x07, 0x1A, 0xEE}               //Main init sequence
#define SI {4, 0x00, 0x00, 0x1C, 0xED}         //Secondary init sequence (after ignition)
#define MRB_2 {3, 0x00, 0x1E, 0xEC}            //Master Request Broadcast version 2

#define C1_1 {5, 0xC1, 0x1B, 0x7F, 0x01, 0x08} //Respond with c1_init_1
#define C1_2 {5, 0xC1, 0x1D, 0x73, 0x01, 0x81} //Respond with c1_init_2 (text)

#define C2_0 {4, 0xC2, 0x1D, 0x73, 0x00}       //Get next byte (nn) and respond with 10, 0, nn, 0,0 and 14 of 0x20 (possibly text)
#define C2_1 {4, 0xC2, 0x1D, 0x73, 0x01}       //Same as above? Answer 19 bytes (unknown)

#define C3_0 {4, 0xC3, 0x1F, 0x7C, 0x00}       //Respond with c1_init_2 (text)
#define C3_1 {4, 0xC3, 0x1F, 0x7C, 0x01}       //Respond with c1_init_2 (text)
#define C3_2 {4, 0xC3, 0x1F, 0x7C, 0x02}       //Respond with c1_init_2 (text)

#define C5_1 {3, 0xC5, 0x19, 0x73}  //C5, 19, 73, xx, yy. Answer 0x10, xx, yy + free text. End with 00 00 and pad with spaces

#define CMD_1 {3, 0xC0, 0x1B, 0x76}            //Followed by: [00, 92, FF], OR [01, 03 ,FF] OR [02, 05, FF]. Answer 0x10
#define CMD_2 {4, 0xC0, 0x1C, 0x70, 0x02}      //Wait 2 bytes and answer 0x90?
#define CMD_3 {5, 0xC0, 0x1D, 0x76, 0x80, 0x00} //Answer: 0x10, 0x80, 0x92

#define BTN {4, 0xC0, 0x1D, 0x77, 0x81}        //Read next byte which is the button #. Respond with 3 bytes
#define NXT {5, 0xC0, 0x1B, 0x71, 0x80, 0x00}  //Answer 1 byte
#define PRV {5, 0xC0, 0x1B, 0x71, 0x00, 0x00}  //Answer 1 byte
#define SCN {4, 0xC0, 0x1A, 0x74, 0x2A}        //Answer 1 byte

//This list can't be too long. We only have so much time between the received bytes. (approx 700 us)
const byte commands[][6] = {
  MRB_1,  // 0 now we are master and can send stuff (like text) to the display!
  MI,     // 1 main init
  SI,     // 2 sec init (00 1E ED respond 0xC5 !!)
  CMD_1,  // 3 follows: [0, 92, ff], OR [1,3 ,FF] OR [2, 5 FF]
  CMD_2,  // 4 wait 2 bytes and answer 0x90?
  MRB_2,  // 5 alternative master req bc
  CMD_3,  // 6 unknown. Answer: 0x10, 0x80, 0x92
  C1_1,   // 7 respond with c1_init_1
  C1_2,   // 8 respond with c1_init_2 (contains text)
  C3_0,   // 9 respond with c3_init_0
  C3_1,   // 10 respond with c3_init_1
  C3_2,   // 11 respond with c3_init_2
  C2_0,   // 12 get next byte (nn) and respond with 10, 0, nn, 0,0 and 14 of 0x20
  C2_1,   // 13
  C5_1,   // 14
  BTN,    // 15
  NXT,    // 16
  PRV,    // 17
  SCN     // 18
};

const byte listLen = 19; //how many rows in the above array
/*
---------------------------------- DEFINITIONS & COMMANDS EXPLANATION ----------------------------------

This section defines all constants, pins, variables, and known MELBUS commands.

1. Pin definitions:
   - MELBUS uses 3 lines: CLOCK (D2), DATA (D3), BUSY (D4).
   - Other Arduino pins are assigned for extra controls:
       next/prev/play buttons, volume up/down, and LED outputs (RGB + misc).

2. Global/volatile variables:
   - melbus_ReceivedByte: stores last received byte (set by interrupt).
   - melbus_Bitposition: tracks which bit is being read (7 → 0).
   - byteIsRead: flag set true when a full byte is received.
   - byteToSend: used globally to hold the next byte for sending.
   - Connected: tracks if HU has accepted/init’d this device.

3. MELBUS IDs:
   - BASE_ID: used when HU sends commands.
   - RESPONSE_ID: used when this device responds.
   - MASTER_ID: used during master handshake.

4. Text handling:
   - textHeader: header bytes for text messages.
   - textRow: row number on HU display to use for custom text.
   - customText: a custom message (36 chars max).
   - textLine[4][36]: startup text lines shown by default.

5. Initialization sequences:
   - C1_Init, C2_Init, C3_Init arrays: 
     Predefined byte sequences that HU expects at different init stages.

6. Commands definition:
   - Each command is defined as a byte array, where:
       First byte = length of command,
       Following bytes = actual MELBUS command sequence.
   - Examples:
       MRB_1: Master Request Broadcast
       MI: Main Init
       CMD_1..3: Various control commands
       C1/C2/C3: Init responses with text/data
       C5_1: Text response with free text field
       BTN: Handles HU button presses
       NXT/PRV/SCN: Track control commands

7. Command list:
   - commands[][] stores all defined commands.
   - listLen keeps track of how many commands exist (19 total).
   - Incoming bytes are matched against this list during loop().

In short:
- This section sets up hardware pin roles,
- Defines variables for communication,
- Provides init sequences and responses,
- Lists all recognized HU command patterns.

----------------------------------------------------------------------------------------
*/



//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################




void setup() {
  //Disable timer0 interrupt. It's is only bogging down the system. We need speed!
  TIMSK0 &= ~_BV(TOIE0);

  //All lines are idle HIGH
  pinMode(MELBUS_DATA, INPUT_PULLUP);
  pinMode(MELBUS_CLOCKBIT, INPUT_PULLUP);
  pinMode(MELBUS_BUSY, INPUT_PULLUP);
  pinMode(nextPin, OUTPUT);
  pinMode(prevPin, OUTPUT);
  pinMode(playPin, OUTPUT);
  digitalWrite(nextPin, LOW);
  digitalWrite(prevPin, LOW);
  digitalWrite(playPin, LOW);
  //set analog pins to off
  for (byte i = 14; i < 21; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }

  //Initiate serial communication to debug via serial-usb (arduino)
  //Better off without it when things work.
  //Serial printing takes a lot of time!!
  Serial.begin(115200);
  Serial.print("Calling HU");

  //Activate interrupt on clock pin
  attachInterrupt(digitalPinToInterrupt(MELBUS_CLOCKBIT), MELBUS_CLOCK_INTERRUPT, RISING);

  //Call function that tells HU that we want to register a new device
  melbusInitReq();
}

/*
---------------------------------- SETUP EXPLANATION ----------------------------------

This runs once when Arduino powers up. It prepares pins, disables unused features, 
and starts communication with the HU.

1. Disable timer0 interrupt:
   - Timer0 normally runs millis()/delay().
   - We don’t need it and it slows down communication, so we turn it off.

2. Configure MELBUS pins:
   - MELBUS lines (DATA, CLOCK, BUSY) are inputs with pullups (idle HIGH).
   - Control pins (next, prev, play) are outputs, set LOW (inactive).
   - Also set analog pins A0–A6 (14–20) as outputs and LOW to avoid noise.

3. Start Serial (debug):
   - Begin at 115200 baud for logging/debugging.
   - Print "Calling HU" (for debug only – serial prints slow things down).

4. Enable interrupts on MELBUS clock:
   - Attach an interrupt handler (MELBUS_CLOCK_INTERRUPT) to the MELBUS clock line.
   - This means whenever the clock goes HIGH, the interrupt will capture bits/bytes.

5. Send initial registration request:
   - Call melbusInitReq() to let the HU know we want to register as a new device.

In short:
- Disable timer0 for speed,
- Set up MELBUS and control pins,
- Enable debug serial,
- Attach clock interrupt,
- Ask HU to register us.

----------------------------------------------------------------------------------------
*/





//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################
//####################################################################################






//Main loop
void loop() {
  static byte byteCounter = 1;  //keep track of how many bytes is sent in current command
  static byte lastByte = 0;     //used to copy volatile byte to register variable. See below
  static byte matching[listLen];     //Keep track of every matching byte in the commands array
  static byte runOnce = 50;
  byte melbus_log[99];  //main init sequence is 61 bytes long...
  bool flag = false;
  bool BUSY = PIND & (1 << MELBUS_BUSY);

  //check BUSY line active (active low)
  while (!BUSY) {
    //Transmission handling here...
    if (byteIsRead) {
      byteIsRead = false;
      lastByte = melbus_ReceivedByte; //copy volatile byte to register variable
      //Well, since HU is talking to us we might call it a conversation.
      melbus_log[byteCounter - 1] = lastByte;
      //Loop though every command in the array and check for matches. (No, don't go looking for matches now)
      for (byte cmd = 0; cmd < listLen; cmd++) {
        //check if this byte is matching
        if (lastByte == commands[cmd][byteCounter]) {
          matching[cmd]++;
          //check if a complete command is received, and take appropriate action
          if ((matching[cmd] == commands[cmd][0]) && (byteCounter == commands[cmd][0])) {
            byte cnt = 0;
            byte b1, b2;
            switch (cmd) {

              //0, MRB_1
              case 0:
                //wait for master_id and respond with same
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == MASTER_ID) {
                      byteToSend = MASTER_ID;
                      SendByteToMelbus();
                      SendText();
                      break;
                    }
                  }
                }
                break;

              //1, MAIN INIT
              case 1:
                //wait for base_id and respond with response_id
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == BASE_ID) {
                      byteToSend = RESPONSE_ID;
                      SendByteToMelbus();
                      break;
                    }
                  }
                }
                Serial.println("main init");
                break;

              //2, Secondary INIT
              case 2:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == BASE_ID) {
                      byteToSend = 0xC5;
                      SendByteToMelbus();
                      break;
                    }
                  }
                }
                Serial.println("SI");
                break;

              //CMD_1. answer 0x10
              case 3:
                // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    cnt++;
                  }
                  if (cnt == 2) {
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    break;
                  }
                }
                Serial.println("ack c0 1b 76");
                break;


              //CMD_1. power on?
              case 4:
                // {0xC0, 0x1C, 0x70, 0x02} we respond 0x90;
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    cnt++;
                  }
                  if (cnt == 2) {
                    byteToSend = 0x90;
                    SendByteToMelbus();
                    break;
                  }
                }
                Serial.println("power?");
                break;

              //MRB_2
              case 5:
                // {00 1E EC };
                //wait for master_id and respond with same
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == MASTER_ID) {
                      byteToSend = MASTER_ID;
                      SendByteToMelbus();
                      SendText();
                      //SendTextI2c();
                      break;
                    }
                  }
                }
                break;

              // CMD_3,  // 6 unknown. Answer: 0x10, 0x80, 0x92
              case 6:
                byteToSend = 0x10;
                SendByteToMelbus();
                byteToSend = 0x80;
                SendByteToMelbus();
                byteToSend = 0x92;
                SendByteToMelbus();
                Serial.println("cmd3");
                break;

              //C1_1,    7 respond with c1_init_1
              case 7:
                for (byte i = 0; i < SO_C1_Init_1; i++) {
                  byteToSend = C1_Init_1[i];
                  SendByteToMelbus();
                }
                Serial.println("\nC1_1");
                break;

              //C1_2,   8 respond with c1_init_2 (contains text)
              case 8:
                for (byte i = 0; i < SO_C1_Init_2; i++) {
                  byteToSend = C1_Init_2[i];
                  SendByteToMelbus();
                }
                Serial.println("\nC1_2");
                break;

              //  C3_0,    9 respond with c3_init_0
              case 9:
                for (byte i = 0; i < SO_C3_Init_0; i++) {
                  byteToSend = C3_Init_0[i];
                  SendByteToMelbus();
                }
                Serial.println("\nC3 init 0");
                break;

              //C3_1,    10 respond with c3_init_1
              case 10:
                for (byte i = 0; i < SO_C3_Init_1; i++) {
                  byteToSend = C3_Init_1[i];
                  SendByteToMelbus();
                }
                Serial.println("\nC3 init 1");
                break;

              //C3_2,   11 respond with c3_init_2
              case 11:
                for (byte i = 0; i < SO_C3_Init_2; i++) {
                  byteToSend = C3_Init_2[i];
                  SendByteToMelbus();
                }
                Serial.println("\nC3 init 2");
                break;

              //   C2_0,    12 get next byte (nn) and respond with 10, 0, nn, 0,0 and 14 of 0x20
              // possibly a text field?
              case 12:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = melbus_ReceivedByte;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x20;
                    for (byte b = 0; b < 14; b++) {
                      SendByteToMelbus();
                    }
                    break;
                  }
                }
                Serial.print("C2_0: ");
                Serial.println(melbus_ReceivedByte, HEX);
                break;

              //C2_1,    13 respond as 12
              case 13:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    byteToSend = 0x01;
                    SendByteToMelbus();
                    byteToSend = melbus_ReceivedByte;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x20;
                    for (byte b = 0; b < 14; b++) {
                      SendByteToMelbus();
                    }
                    break;
                  }
                }
                Serial.print("C2_1: ");
                Serial.println(melbus_ReceivedByte, HEX);
                break;

              //define C5_1 {5, 0xC5, 0x19, 0x73}
              //C5, 19, 73, xx, yy. Answer 0x10, xx, yy + free text.
              //End with 00 00 and pad with spaces
              case 14:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b1 = melbus_ReceivedByte;
                    break;
                  }
                }
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b2 = melbus_ReceivedByte;
                    break;
                  }
                }
                byteToSend = 0x10;
                SendByteToMelbus();
                byteToSend = b1;
                SendByteToMelbus();
                byteToSend = b2;
                SendByteToMelbus();
                for (byte b = 0; b < 36; b++) {
                  byteToSend = textLine[b2 - 1][b];
                  SendByteToMelbus();
                }
                Serial.print("C5_1: ");
                break;

              //BTN
              case 15:
                //wait for next byte to get CD number
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b1 = melbus_ReceivedByte;
                    break;
                  }
                }
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                SendByteToMelbus();
                SendByteToMelbus();

                switch (b1) {
                  //0x1 to 0x6 corresponds to cd buttons 1 to 6 on the HU (650) (SAT 1)
                  //7-13 on SAT 2, and 14-20 on SAT 3
                  case 0x1:
                    //toggleOutput(LEDMISC1); //turn on/off one output pin.
                    break;
                  case 0x2:
                    toggleOutput(LEDLEFT); //turn on/off one output pin.
                    break;
                  case 0x3:
                    toggleOutput(LEDRIGHT); //turn on/off one output pin.
                    break;
                  case 0x4:
                    toggleOutput(LEDR); //turn on/off one output pin.
                    break;
                  case 0x5:
                    toggleOutput(LEDG); //turn on/off one output pin.
                    break;
                  case 0x6:
                    toggleOutput(LEDB); //turn on/off one output pin.
                    break;
                }


                Serial.print("you pressed CD #");
                Serial.println(b1);
                break;

              //NXT
              case 16:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                nextTrack();
                break;

              //PRV
              case 17:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                prevTrack();
                break;

              //SCN
              case 18:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                play();
                break;


            } //end switch
            Connected = true;
            break;    //bail for loop. (Not meaningful to search more commands if one is already found)
          } //end if command found
        } //end if lastbyte matches
      }  //end for cmd loop
      byteCounter++;
    }  //end if byteisread
    //Update status of BUSY line, so we don't end up in an infinite while-loop.
    BUSY = PIND & (1 << MELBUS_BUSY);
    if (BUSY) {
      flag = true; //used to execute some code only once between transmissions
    }
  }


  //Do other stuff here if you want. MELBUS lines are free now. BUSY = IDLE (HIGH)
  //Don't take too much time though, since BUSY might go active anytime, and then we'd better be ready to receive.
  //Printing transmission log (incoming, before responses)
  if (flag) {
    for (byte b = 0; b < byteCounter - 1; b++) {
      Serial.print(melbus_log[b], HEX);
      Serial.print(" ");
    }
    Serial.println();
    if (runOnce >= 1) {
      runOnce--;
    }

  } else {
    //Serial.print(".");
  }

  //Reset stuff
  byteCounter = 1;
  melbus_Bitposition = 7;
  for (byte i = 0; i < listLen; i++) {
    matching[i] = 0;
  }

  //    if ((!Connected) && flag) {
  //      Serial.println("init...");
  //      melbusInitReq();
  //    }

  if (Serial.available() > 0) {
    Serial.read();
    Serial.println("\nText");
    //next run, we want to send text!
    reqMaster();
  }

  if (runOnce == 1) {
    reqMaster();
  }

  flag = false; //don't print during next loop. Wait for new message to arrive first.
}

/*
---------------------------------- MAIN LOOP EXPLANATION ----------------------------------

This loop handles communication over the MELBUS protocol (used by the head unit HU).  
The flow works like this:

1. Check if the BUSY line is active (LOW = HU is sending data).
   - While BUSY is LOW, we listen for incoming bytes.

2. When a byte is received:
   - Copy it into a local variable (lastByte).
   - Store it in melbus_log[] (keeps a record of the whole transmission).
   - Compare the received byte with all known command patterns in commands[][].
     -> If it matches the expected position of a command, increase its "matching" counter.

3. If a complete command is detected:
   - A switch(cmd) decides what to do depending on which command it was.
   - Each case sends the required response bytes back to the HU (using SendByteToMelbus()).
   - Some commands also trigger extra functions (SendText, toggle LEDs, nextTrack, prevTrack, etc.).
   - After a valid command is handled, mark "Connected = true" and break.

4. When HU stops talking (BUSY goes HIGH again):
   - Print the log of received bytes to Serial (for debugging).
   - Reset byte counters and matching[] for the next transmission.

5. Extra tasks while bus is idle:
   - If data is available on Serial, trigger reqMaster() to request text sending.
   - If runOnce reaches 1, also call reqMaster() (first-time setup).

6. Finally, reset the flag so we only print once per transmission cycle.

In short:
- While HU talks: receive bytes, check for command matches, respond accordingly.
- When HU stops: print log, reset counters, possibly send our own requests.

--------------------------------------------------------------------------------------------
*/


//Notify HU that we want to trigger the first initiate procedure to add a new device (CD-CHGR//not really, i think its sirius sat)) by pulling BUSY line low for 1s
void melbusInitReq() {
  //Serial.println("conn");
  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);

  // Wait until Busy-line goes high (not busy) before we pull BUSY low to request init
  while (digitalRead(MELBUS_BUSY) == LOW) {}
  delayMicroseconds(20);

  pinMode(MELBUS_BUSY, OUTPUT);
  digitalWrite(MELBUS_BUSY, LOW);
  //timer0 is off so we have to do a trick here
  for (unsigned int i = 0; i < 12000; i++) delayMicroseconds(100);

  digitalWrite(MELBUS_BUSY, HIGH);
  pinMode(MELBUS_BUSY, INPUT_PULLUP);
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);
}


//This is a function that sends a byte to the HU - (not using interrupts)
//SET byteToSend variable before calling this!!
void SendByteToMelbus() {
  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);

  //Convert datapin to output
  //pinMode(MELBUS_DATA, OUTPUT); //To slow, use DDRD instead:
  DDRD |= (1 << MELBUS_DATA);

  //For each bit in the byte
  for (char i = 7; i >= 0; i--)
  {
    while (PIND & (1 << MELBUS_CLOCKBIT)) {} //wait for low clock
    //If bit [i] is "1" - make datapin high
    if (byteToSend & (1 << i)) {
      PORTD |= (1 << MELBUS_DATA);
    }
    //if bit [i] is "0" - make datapin low
    else {
      PORTD &= ~(1 << MELBUS_DATA);
    }
    while (!(PIND & (1 << MELBUS_CLOCKBIT))) {}  //wait for high clock
  }
  //Let the value be read by the HU
  delayMicroseconds(20);
  //Reset datapin to high and return it to an input
  //pinMode(MELBUS_DATA, INPUT_PULLUP);
  PORTD |= 1 << MELBUS_DATA;
  DDRD &= ~(1 << MELBUS_DATA);

  //We have triggered the interrupt but we don't want to read any bits, so clear the flag
  EIFR |= 1 << INT_NUM;
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);
}


//This method generates our own clock. Used when in master mode.
void SendByteToMelbus2() {
  delayMicroseconds(700);
  //For each bit in the byte
  //char, since it will go negative. byte 0..255, char -128..127
  //int takes more clockcycles to update on a 8-bit CPU.
  for (char i = 7; i >= 0; i--)
  {
    delayMicroseconds(7);
    PORTD &= ~(1 << MELBUS_CLOCKBIT);  //clock -> low
    //If bit [i] is "1" - make datapin high
    if (byteToSend & (1 << i)) {
      PORTD |= (1 << MELBUS_DATA);
    }
    //if bit [i] is "0" - make datapin low
    else {
      PORTD &= ~(1 << MELBUS_DATA);
    }
    //wait for output to settle
    delayMicroseconds(5);
    PORTD |= (1 << MELBUS_CLOCKBIT);   //clock -> high
    //wait for HU to read the bit
  }
  delayMicroseconds(20);
}



//Global external interrupt that triggers when clock pin goes high after it has been low for a short time => time to read datapin
void MELBUS_CLOCK_INTERRUPT() {
  //Read status of Datapin and set status of current bit in recv_byte
  //if (digitalRead(MELBUS_DATA) == HIGH) {
  if ((PIND & (1 << MELBUS_DATA))) {
    melbus_ReceivedByte |= (1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "1"
  }
  else {
    melbus_ReceivedByte &= ~(1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "0"
  }

  //if all the bits in the byte are read:
  if (melbus_Bitposition == 0) {
    //set bool to true to evaluate the bytes in main loop
    byteIsRead = true;

    //Reset bitcount to first bit in byte
    melbus_Bitposition = 7;
  }
  else {
    //set bitnumber to address of next bit in byte
    melbus_Bitposition--;
  }
}


void SendText() {
  //Serial.println("sendtext()");

  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);


  //Convert datapin and clockpin to output
  //pinMode(MELBUS_DATA, OUTPUT); //To slow, use DDRD instead:
  PORTD |= 1 << MELBUS_DATA;      //set DATA input_pullup => HIGH when output (idle)
  DDRD |= (1 << MELBUS_DATA);     //set DATA as output
  PORTD |= 1 << MELBUS_CLOCKBIT;  //set CLK input_pullup => HIGH when output (idle)
  DDRD |= (1 << MELBUS_CLOCKBIT); //set CLK as output

  //send header
  for (byte b = 0; b < 4; b++) {
    byteToSend = textHeader[b];
    SendByteToMelbus2();
  }

  //send which row to show it on
  byteToSend = textRow;
  SendByteToMelbus2();

  //send text
  for (byte b = 0; b < 36; b++) {
    byteToSend = customText[b];
    SendByteToMelbus2();
  }

  DDRD &= ~(1 << MELBUS_CLOCKBIT);//back to input (PULLUP since we clocked in the last bit with clk = high)
  //Reset datapin to high and return it to an input
  //pinMode(MELBUS_DATA, INPUT_PULLUP);
  PORTD |= 1 << MELBUS_DATA;  //this may or may not change the state, depending on the last transmitted bit
  DDRD &= ~(1 << MELBUS_DATA);//release the data line

  //Clear INT flag
  EIFR |= 1 << INT_NUM;
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);

  Serial.println("finished");
}



void reqMaster() {
  DDRD |= (1 << MELBUS_DATA); //output
  PORTD &= ~(1 << MELBUS_DATA);//low
  delayMicroseconds(700);
  delayMicroseconds(700);
  delayMicroseconds(800);
  PORTD |= (1 << MELBUS_DATA);//high
  DDRD &= ~(1 << MELBUS_DATA); //back to input_pullup
}




/*
---------------------------------- toggleOutput() ----------------------------------

This helper function toggles (flips) the state of a given pin:

1. Reads the current state of the pin (HIGH or LOW).
2. Writes the opposite value back to the pin (LOW → HIGH, HIGH → LOW).
3. Prints a debug message to Serial with the pin number.

Used to turn LEDs or outputs on/off when buttons/commands are received.

------------------------------------------------------------------------------------
*/
void toggleOutput(byte pinNumber) {
  digitalWrite(pinNumber, !digitalRead(pinNumber));
  Serial.print("toggled a pin: ");
  Serial.println(pinNumber);
}

//Simulate button presses on the BT module. 200 ms works good. Less is not more in this case...
void nextTrack() {
  digitalWrite(nextPin, HIGH);
  for (byte i = 0; i < 200; i++)
    delayMicroseconds(1000);
  digitalWrite(nextPin, LOW);
}

void prevTrack() {
  digitalWrite(prevPin, HIGH);
  for (byte i = 0; i < 200; i++)
    delayMicroseconds(1000);
  digitalWrite(prevPin, LOW);
}

void play() {
  digitalWrite(playPin, HIGH);
  for (byte i = 0; i < 200; i++)
    delayMicroseconds(1000);
  digitalWrite(playPin, LOW);
}

//Happy listening AND READING, hacker!


