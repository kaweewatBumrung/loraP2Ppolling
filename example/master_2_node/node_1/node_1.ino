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

// this node EUI (you may write it as 0x...)

// MUST BE UNIQUE ACROSS ALL NODE IN ALL NETWORKS for the system to work

// if it not unique master can't distinguish between each node
const uint64_t      this_node_EUI = 0xFEDCBA1234567890;  // in HEX

// Hardware pins
const int           RAK4200_RX  = 14;
const int           RAK4200_TX  = 13;
const int           RAK4200_RES = 33;
const int           Exled = 26;

// SF MUST match between master and node, only allow SF7 - SF10
const int           SF = 8;

// power in dBm, no more than 17dBm (50mW)
const int           power_dBm = 11;

// (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin)
P2PpollingClass P2Ppolling(RAK4200_RX, RAK4200_TX, RAK4200_RES);

void setup() {
  // Must call this before setXXX, FSM_polling
  // select master or node, with acknowledgement or without
  // and set SF, power_dBm
  P2Ppolling.P2Ppolling_begin(select_node, ACK_mode, SF, power_dBm);

  // add this node EUI (for node)
  P2Ppolling.setNode_EUI(this_node_EUI);

  // set function to call (for node)
  P2Ppolling.setReadbattfunc(readBatt);
  P2Ppolling.setReadmoisfunc(readMoisture);
  P2Ppolling.setReadtempfunc(readTemp);

  // set external led (optional) (for node)
  P2Ppolling.setExled(Exled);
}

void loop() {
  // you have to make loop run as quick as possible

  // will call finite state machine of selected type (master or node)
  P2Ppolling.FSM_polling();
}

// function get call when node waiting for master (State 1)
void readBatt (uint8_t& _batt)
{
  // sensor reading here or more complex with it own finite state machine

  // here this is only a dummy for simple testing the library
  _batt = 255;
  Serial.println("fix batt 255");
}

// function get call when node waiting for master (State 1)
void readTemp (uint16_t& _temperature)
{
  // sensor reading here or more complex with it own finite state machine

  // allow value MUST be 0 - 4095 (only 12bits for 3 digit HEX)

  // here this is only a dummy for simple testing the library
  _temperature = random(0, 2000);
  Serial.println("(random)temp : " + (String)_temperature);
}

// function get call when node waiting for master (State 1)
void readMoisture (uint16_t& _moisture)
{
  // sensor reading here or more complex with it own finite state machine

  // allow value MUST be 0 - 4095 (only 12bits for 3 digit HEX)

  // here this is only a dummy for simple testing the library
  _moisture = random(0, 2000);
  Serial.println("(random)moisture : " + (String)_moisture);
}
