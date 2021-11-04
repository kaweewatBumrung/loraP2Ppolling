/*
  write by: kaweewat bumrung
  1/11/2021 dd/mm/yyyy
  loraP2Ppolling library version 1.0.0
*/

#include "loraP2Ppolling.h"

#define select_master               1
#define select_node                 2
#define ACK_mode                    1
#define NO_ACK_mode                 0

// Hardware pins
const int           RAK4200_RX  = 14;
const int           RAK4200_TX  = 13;
const int           RAK4200_RES = 33;
const int           Exled = 26; // (optinal) for node only

// SF MUST match between master and node
// time on air MUST not exceeded max dwell time for AS923 -> 400ms
// only allow SF7 - SF10 (SF11, SF12 will exceeded max dwell time)
const int           SF = 8;

// power in dBm, no more than 17dBm (50mW)
const int           power_dBm = 11;

// node EUI to accept (you may write it as 0x...)

// MUST BE UNIQUE ACROSS ALL NODE IN ALL NETWORKS for the system to work

// if it not unique master can't distinguish between each node
const uint64_t      test_EUI_1 = 0xFEDCBA1234567890;  // 16 digits HEX
const uint64_t      test_EUI_2 = 0xABCDEF0987654321;  // 16 digits HEX

// P2PpollingClass (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin);
P2PpollingClass P2Ppolling(RAK4200_RX, RAK4200_TX, RAK4200_RES);

void setup() {
  // Must call this before setXXX, FSM_polling
  // select master or node, with acknowledgement or without
  // and set SF, power_dBm
  P2Ppolling.P2Ppolling_begin(select_master, ACK_mode, SF, power_dBm);

  // add node EUI to accept (for master)
  P2Ppolling.addtoAccept_EUI(test_EUI_1);
  P2Ppolling.addtoAccept_EUI(test_EUI_2);

  // set function to call (for master)
  P2Ppolling.setPrintdatafunc(printData);

/*
  you MUST calculate polling interval yourself 
  in THAILAND we have limit at no more than 1% duty cycle
  see this library NOTE for how to calculate polling interval
*/
  
  // set interval (ms), if not set default to 10s (for master)
  P2Ppolling.setPolling_interval(8000);
}

void loop() {
  // you have to make loop run as quick as possible

  // will call finite state machine of selected type (master or node)
  P2Ppolling.FSM_polling();
}

/*
  for read node data you can use 
  e.g. P2Ppolling.getBatt(P2Ppolling.EUI_toAddr(EUI));
  and use it in other part of your program
*/

// function get call at the end of each node polling (1 time per node) 
// get call both success and not success
void printData (uint8_t _addr, int success)
{
  // here this is only a dummy for simple testing the library

  Serial.print("node address : ");
  Serial.println(_addr);
  Serial.print("success : ");
  if (success == 1) {
    Serial.println("OK");
  } else Serial.println("NOT OK");
  
  Serial.print("node_batt        : ");
  Serial.println(P2Ppolling.getBatt(_addr));

  Serial.print("node_moisture    : ");
  Serial.println(P2Ppolling.getMoisture(_addr));

  Serial.print("node_temperature : ");
  Serial.println(P2Ppolling.getTemp(_addr));
}
