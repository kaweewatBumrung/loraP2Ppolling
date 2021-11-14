// Copyright (c) kaweewatBumrung. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/*
  loraP2Ppolling.cpp
  Part of loraP2Ppolling arduino library version 1.1.0
  Author: kaweewat bumrung
  date: 4/11/2021 (dd/mm/yyyy)
  arduino IDE 1.8.15
  ESP32 arduino core 2.0.1

  to use with ESP32 and RAK4200

  hardware from ETT
  http://www.etteam.com/prodIOT/LORA-RAK4200-AS923-MODULE/index.html
  this file version 1.1.0
*/

#include "loraP2Ppolling.h"
#include "mod_ETT_RAK4200.h"
#include <stdio.h>

//https://github.com/rlogiacco/CircularBuffer
#define CIRCULAR_BUFFER_XS
#include <CircularBuffer.h>

#define SerialDebug                 Serial
#define SerialLora                  Serial1

#define select_master               1
#define select_node                 2

#define totalChannels               14
#define FREQ_CH0                    920200000
#define FREQ_CH1                    920400000
#define FREQ_CH2                    920600000
#define FREQ_CH3                    920800000
#define FREQ_CH4                    921000000
#define FREQ_CH5                    921200000
#define FREQ_CH6                    921400000
#define FREQ_CH7                    921600000
#define FREQ_CH8                    921800000
#define FREQ_CH9                    922000000
#define FREQ_CH10                   922200000
#define FREQ_CH11                   922400000
#define FREQ_CH12                   922600000
#define FREQ_CH13                   922800000

// from semtech lora modem calculator tool (round up to ms)
// see datasheet (sx1276 for RAK4200) for full equation
#define timeOnair_SF7_4byte         (29)
#define timeOnair_SF7_8byte         (46)
#define timeOnair_SF7_12byte        (54)

#define timeOnair_SF8_4byte         (58)
#define timeOnair_SF8_8byte         (75)
#define timeOnair_SF8_12byte        (91)

#define timeOnair_SF9_4byte         (116)
#define timeOnair_SF9_8byte         (149)
#define timeOnair_SF9_12byte        (182)

#define timeOnair_SF10_4byte        (232)
#define timeOnair_SF10_8byte        (297)
#define timeOnair_SF10_12byte       (297)

// time spent (blocking) in RAK4200 at com (ms)
#define dura_rk_initP2P_mode        (11)
#define dura_rk_initP2P             (250)

// just set some duration up, it will be calculate with SF later
int acktimeOut                      = 300;   // ~300 use dura_rk_initP2P_mode and timeOnair_SF8_8byte
int joinrequest_ans_timeOut         = 2000;  // ~()
int netAddr_answer_timeOut          = 2000;  // ~()
int CH_Sweep_timeOut                = 9100;  // ~9100 ~(100 * timeOnair_SF8_12byte)
int netAddr_negotiate_timeOut       = 5800;  // ~5800 ~(100 * timeOnair_SF8_4byte)

// address for assign in the range of 32 - 254, total of 223
const uint8_t   startOffset = 32;

const uint8_t   _addr_ask_netAddr       = 1;
const uint8_t   _addr_answer_netAddr    = 2;
const uint8_t   _addr_declare_netAddr   = 3;
const uint8_t   _addr_denial_netAddr    = 4;
const uint8_t   _addr_joinRequest       = 5;
const uint8_t   _addr_joinAccept        = 6;
const uint8_t   _addr_masterAddr        = 16; // fix, 1 master per network
const uint8_t   _addr_broadcast         = 255;

// frameType
const uint8_t   _type_M_ask_N_noACK     = 1;
const uint8_t   _type_M_ask_N_ACK       = 2;
const uint8_t   _type_N_answer_noACK    = 3;
const uint8_t   _type_N_answer_ACK      = 4;
const uint8_t   _type_M_ACK             = 5;
const uint8_t   _type_M_broadcast       = 6;  // not using it as of now
const uint8_t   _type_ask_netAddr       = 16;
const uint8_t   _type_answer_netAddr    = 17;
const uint8_t   _type_N_answer_netAddr  = 18;
const uint8_t   _type_declare_netAddr   = 19;
const uint8_t   _type_denial_netAddr    = 20;
const uint8_t   _type_joinRequest       = 32;
const uint8_t   _type_joinAccept        = 33;
const uint8_t   _type_join_N_ACK        = 34;
const uint8_t   _type_join_M_ACK        = 35;
const uint8_t   _type_nodeRemove        = 48;
const uint8_t   _type_remove_N_ACK      = 49;
const uint8_t   _type_remove_M_ACK      = 50;

// network address
// for assign using 32 - 254
const uint8_t   _netAddr_ask            = 1;
const uint8_t   _netAddr_joinRequest    = 10;
const uint8_t   _netAddr_default        = 255; // if can't find available netAddr

CircularBuffer<uint64_t, 223> joinRequestEUI_FIFO;
CircularBuffer<uint64_t, 223> toAcceptEUI_FIFO;

RAK4200 RakLoRa(SerialLora, SerialDebug);

// public Constructor
P2PpollingClass::P2PpollingClass (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin) 
{
  SerialDebug.begin(115200);
  if (numInstance == 0) {
      moduleInitialize (lora_rx_pin, lora_tx_pin, lora_reset_pin);
  }
  else Serial.println("ERROR single instance only");
}

// begin, start normal config 
// (master = 1, node = 2), (no_ACK = 0, ACK =1), (_SF = 7 - 12), (_power = 5 - 17)
bool P2PpollingClass::P2Ppolling_begin (int select_master_node, int ACK_mode, int _SF, int _power)
{
  bool to_return = false;
  String _Buffer;
  uint32_t c;
  // reserve memory for String, try to avoid heap fragmentation (in bytes)
  payloadString.reserve(13);          // full payload
  ComBuffer.reserve(257);             // full uart RX buffer
  ComnetAddrNegotiate.reserve(257);   // full uart RX buffer

  if (select_master_node == select_master) 
  {
    set_master_or_node = select_master_node;
    Serial.println("select_master");
  }
  else if (select_master_node == select_node)
  {
    ComRemove .reserve(50); // ~(at com + payload)
    set_master_or_node = select_master_node;
    Serial.println("select_node");
  }
  // wrong select_master_node
  else
  {
    Serial.println("ERROR can't begin");
    return to_return;
  }

  // select master or node
  if ((ACK_mode == 0) || (ACK_mode == 1)) {
    nodetoACK = ACK_mode;
  }

  // spreading factor (SF) must not exceeded max dwell time
  // for AS923 -> 400ms , only allow SF <= 10
  if ((_SF >= 7) && (_SF <= 10)) {
    SF = _SF;
  }

  // for AS923 power <= 50mW (~17dBm)
  if ((_power >= 5) && (_power <= 17)) {
    TX_Power = _power;
  }

  // calculate time out from SF
  switch (_SF)
  {
    case 7:
      {
        acktimeOut = timeOnair_SF7_8byte + (2*dura_rk_initP2P_mode) + 100; // 168
        CH_Sweep_timeOut = 99 * timeOnair_SF7_12byte; // 5346
        netAddr_negotiate_timeOut = 99 * timeOnair_SF7_4byte; // 2871
        break;
      }
    case 8:
      {
        acktimeOut = timeOnair_SF8_8byte + (2*dura_rk_initP2P_mode) + 100; // 197;
        CH_Sweep_timeOut = 99 * timeOnair_SF8_12byte;  // 9009
        netAddr_negotiate_timeOut = 99 * timeOnair_SF8_4byte;  // 5742
        break;
      }
    case 9:
      {
        acktimeOut = timeOnair_SF9_8byte + (2*dura_rk_initP2P_mode) + 100; // 271;
        CH_Sweep_timeOut = 99 * timeOnair_SF9_12byte;  // 18018
        netAddr_negotiate_timeOut = 99 * timeOnair_SF9_4byte;  // 11484
        break;
      }
    case 10:
      {
        acktimeOut = timeOnair_SF10_8byte + (2*dura_rk_initP2P_mode) + 100; // 419;
        CH_Sweep_timeOut = 99 * timeOnair_SF10_12byte;  // 29403
        netAddr_negotiate_timeOut = 99 * timeOnair_SF10_4byte;  // 22968
        break;
      }
    default:
      {
        acktimeOut = timeOnair_SF8_8byte + (2*dura_rk_initP2P_mode) + 100; // 197;
        CH_Sweep_timeOut = 99 * timeOnair_SF8_12byte;  // 9009
        netAddr_negotiate_timeOut = 99 * timeOnair_SF8_4byte;  // 5742
      }
  }

  T_waitforJoin = (totalChannels * CH_Sweep_timeOut) + acktimeOut;

  Serial.print("acktimeOut : ");
  Serial.println(acktimeOut);
  Serial.print("CH_Sweep_timeOut : ");
  Serial.println(CH_Sweep_timeOut);
  Serial.print("netAddr_negotiate_timeOut : ");
  Serial.println(netAddr_negotiate_timeOut);

  // set freq to FREQ_CH0
  CH_FREQ = FREQ_CH0;

  SerialDebug.println("Start Config RAK4200...");
  while (!SerialLora);

  // wait for Initialization OK
  c = millis();
  do { _Buffer = RakLoRa.rk_recvP2PData();
  } while ((_Buffer.indexOf("Initialization OK") == -1) && (millis() - c <= 1000));

  // Initial LoRa Work Mode = LoRaP2P Mode
  if (RakLoRa.rk_setWorkingMode(1)) {
    SerialDebug.println(F("set Work Mode P2P"));
  }
  else 
  {
    SerialDebug.println(F("set Work Mode failed"));

    // Reset
    digitalWrite(LORA_RES_PIN, LOW);
    delay(500);
    digitalWrite(LORA_RES_PIN, HIGH);

    // Wait Power on Ready
    String ack = "";
    uint32_t TS = millis();
    while (true)
    {
      ack = RakLoRa.rk_recvData();
      if (ack != NULL) {
        //SerialDebug.println(ack);
      }
      if (ack.indexOf("Initialization OK") >= 0) {
        break;
      }
      // wait up to 1000ms
      if ((millis() - TS) < 1000) {
        Serial.println("ERROR module not respond");
        
        // restart ESP32
        Serial.println("restart");
        ESP.restart();
      }
    }
  }

  SerialDebug.println(F("Init LoRaP2P parameters"));

  // set parameter
  // Freq(HZ), SF(12), BW(0 = 125kHz), CR(1 = 4/5),Preamble 8,Power(dBm) 11 (max 17)
  c = millis();
  if (!RakLoRa.rk_initP2P((String)CH_FREQ, SF, BW, CR, PRlen, TX_Power)) {
    _Buffer;
    do { _Buffer = RakLoRa.rk_recvP2PData();
    } while ((_Buffer.indexOf("OK") == -1) && (millis() - c <= 1000)); //.indexOf return -1 when not found
    Serial.println(_Buffer);
  }

  if (RakLoRa.rk_initP2P_mode(P2P_RECEIVER)) {
    SerialDebug.println(F("RECEIVER Mode"));
  }
  to_return = true;
  return to_return;
}

bool P2PpollingClass::addtoAccept_EUI (uint64_t toAccept_EUI)
{
  return toAcceptEUI_FIFO.unshift(toAccept_EUI);
}

bool P2PpollingClass::setNode_EUI (uint64_t toSet_EUI)
{
  thisNodeEUI = toSet_EUI;
  return true;
}

void P2PpollingClass::setExled (int _Exled)
{
  ExLED = _Exled;
}

bool P2PpollingClass::set_T_waitforJoin (uint32_t _ms)
{
  bool to_return = false;
  Serial.println("set waitforJoin");
  if (_ms >= CH_Sweep_timeOut) {
    T_waitforJoin = _ms;
    to_return = true;
  } else Serial.println("set fail");
  Serial.print("T_waitforJoin : ");
  Serial.println(T_waitforJoin);
  return to_return;
}

bool P2PpollingClass::setPolling_interval (uint32_t _ms)
{
  bool to_return = false;
  Serial.println("set Interval");
  if (_ms >= acktimeOut) {
    T_Interval = _ms;
    to_return = true;
  } else Serial.println("set fail");
  Serial.print("T_Interval : ");
  Serial.println(T_Interval);
  return to_return;
}

// will call finite state machine of selected type (master or node)
int P2PpollingClass::FSM_polling ()
{
  switch (set_master_or_node)
  {
    case select_master: return masterPolling();
    case select_node:   return nodePolling();
    default:            return -1;
  }
}

// finite state machine for master
int P2PpollingClass::masterPolling ()
{
  // _Sink_ variable, use for frame decode
  uint8_t     _Sink_addrReceiver      = 0;
  uint8_t     _Sink_addrSender        = 0;
  uint8_t     _Sink_networkAddr       = 0;
  uint8_t     _Sink_frameType         = 0;
  uint8_t     _Sink_batt8bit          = 0;
  uint16_t    _Sink_moisture16bit     = 0;
  uint16_t    _Sink_temperature16bit  = 0;
  uint64_t    _Sink_EUI               = 0;

  switch (internalState)
  {
    case 1: // wait (interval)
      {
        //Serial.println("S: " + (String)internalState);
        
        if (startMainloop == 0) {  // initialize as 0
          startMainloop = 1;
          timeStamp_loop = millis();
        }

        // to netAddr negotiate
        if (netAddrNegotiate_success == 0) {
          internalState = 30;
          break;
        }

        //Serial.print("start : ");
        //Serial.println(startMainloop);
        //Serial.print("TS : ");
        //Serial.println(timeStamp_loop);

        // wait for node to join
        if ((startMainloop == 1) && (millis() - timeStamp_loop >= T_waitforJoin)) {
          startMainloop = 2;
          Serial.println("wait end");
        }

        // loop with T_Interval
        if (((startMainloop == 2) || (startMainloop == 3))
            && (millis() - timeStamp_loop >= T_Interval)) {
          startMainloop = 3;
          timeStamp_loop = millis();
          internalState = 2;
          tonextNode = 0;
          Serial.println();
          Serial.println("New loop > timeStamp_loop : " + (String)timeStamp_loop);
        }
        
        // check for nonpolling frame ex join request, netAddr Negotiate
        int result = 0;
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) 
        {
          timeStamp = millis();
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          result = frameDecoder (TestString,
                                 _Sink_addrReceiver,
                                 _Sink_addrSender,
                                 _Sink_networkAddr,
                                 _Sink_frameType,
                                 _Sink_batt8bit,
                                 _Sink_moisture16bit,
                                 _Sink_temperature16bit,
                                 _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          // join relate
          if ((_Sink_addrReceiver == _addr_broadcast) 
              && (_Sink_addrSender == _addr_joinRequest) 
              && (_Sink_networkAddr == _netAddr_joinRequest) 
              && (_Sink_frameType == _type_joinRequest))
          {
            for (int i = 0; i <= joinRequestEUI_FIFO.size(); i++)
            { // check duplicate
              if (!(_Sink_EUI == joinRequestEUI_FIFO[i]))
              { // not duplicate
                joinRequestEUI_FIFO.unshift(_Sink_EUI);
                if (joinRequest < 255) joinRequest += 1;
                Serial.print("add joinRequestEUI : ");
                _EUI64_print(joinRequestEUI_FIFO.last());
                break;
              }
            }
          }
        }
        
        // if have join relate process it
        if (joinRequest > 0) {
          internalState = 20;
          break;
        }

        // netAddr Negotiate
        else if ((result == 1) 
                 && ((_Sink_frameType == _type_ask_netAddr) 
                 || (_Sink_frameType == _type_declare_netAddr))) 
        {
          internalState = 25;
          netAddrNegotiate = _Sink_frameType;
          timeStamp_netAddrnegotiate = millis();
          ComnetAddrNegotiate = ComBuffer;
          break;
        }
        if (nodeRemove_flag != 0) {
          Serial.println("nodeRemove");
          internalState = 26;
          break;
        }
        break;
      }
    case 2: // polling
      {
        Serial.println("S: " + (String)internalState);
        uint8_t i;
        // check are we within polling or wait state
        if (tonextNode == 1) {
          i = addrCurrentnode + 1;
        } else i = startOffset;

        // check addr available
        for (; i <= 254; i++)
        {
          //Serial.println((String)"in for i : " + i);
          if (TestBit (i, Addr_bitmask)) {  //TestBit return addr(i) that have batmask == 1
            addrCurrentnode = i;
            break;  // break for loop
          }
          if (i == 254) // when ran out of Addr to check -> to state 1
          {
            Serial.println("endOfpolling");
            internalState = 1;
            tonextNode = 0;
            break;
          }
        }
        
        // if have some other frame, process it before polling next node
        if (joinRequest > 0) {
          internalState = 20;
          break;
        }
        if (netAddrNegotiate != 0) {
          Serial.println("netAddrNegotiate : " + (String)netAddrNegotiate);
          internalState = 25;
          break;
        }
        if (nodeRemove_flag != 0) {
          Serial.println("nodeRemove");
          internalState = 26;
          break;
        }
        // run out of addr break away
        if (internalState == 1) break;

        // process to next node
        numReAck = 0;
        numRetry = 0;
        if (nodetoACK == 1) {
          tosend_frameType = _type_M_ask_N_ACK;   // with ACK
        } else if (nodetoACK == 0) {
          tosend_frameType = _type_M_ask_N_noACK; // without ACK
        } else {
          tosend_frameType = 0;
        }
        tonextNode = 0;
        internalState = 3;
        break;
      }
    case 3: // Encode and send polling
      {
        Serial.println("S: " + (String)internalState);
        frameEncoder (payloadString,
                      payloadCharArray,
                      addrCurrentnode,
                      _addr_masterAddr,
                      thisnetAddr,
                      tosend_frameType);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        if (nodetoACK == 1) {
          internalState = 4;
        }
        else internalState = 7;
        timeStamp = millis();
        break;
      }
    case 4: // ACK type
      {
        Serial.println("S: " + (String)internalState);
        if ((numRetry + numReAck) >= (maxRE + 1)) {
          internalState = 10; // reach maxRE
          break;
        }
        if ((numReAck > 0) && (millis() - timeStamp >= acktimeOut)) {
          internalState = 9; // timeout reach (node receive ACK, didn't retry)
          break;
        }
        if (millis() - timeStamp >= acktimeOut) {
          internalState = 6; // timeout reach (node not send) to retry
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) {
          timeStamp = millis();
          internalState = 8;  // timeout and maxRE not reach, to Decode
        }
        break;
      }
    case 5: // send ACK
      {
        Serial.println("S: " + (String)internalState);
        frameEncoder (payloadString,
                      payloadCharArray,
                      addrCurrentnode,
                      _addr_masterAddr,
                      thisnetAddr,
                      _type_M_ACK);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        numReAck += 1;
        internalState = 4;
        break;
      }
    case 6: // retry
      {
        //Serial.println("S: " + (String)internalState);
        if (numRetry >= 3)
        {
          Serial.println("run out of Retry");
          internalState = 4;
          numRetry = 4;
          break;
        }
        else numRetry += 1;
        Serial.println("Retry " + (String)numRetry);
        frameEncoder (payloadString,
                      payloadCharArray,
                      addrCurrentnode,
                      _addr_masterAddr,
                      thisnetAddr,
                      tosend_frameType);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
          timeStamp = millis();
        }
        internalState = 4;
        break;
      }
    case 7: // no ACK type
      {
        Serial.println("S: " + (String)internalState);
        if (millis() - timeStamp >= acktimeOut) {
          internalState = 10; // timeout reach (node not send)
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) {
          timeStamp = millis();
          internalState = 8;
        }
        break;
      }
    case 8:  // Decode (in polling time slot)
      {
        bool getPolling = false;
        //Serial.println("S: " + (String)internalState);
        Serial.println(ComBuffer);
        int num = ComBuffer.indexOf(':');
        String TestString = ComBuffer;
        TestString.remove(0, ++num);
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        // polling                           
        if ((result == 3) && (_Sink_addrReceiver == _addr_masterAddr)
            && (_Sink_addrSender == addrCurrentnode) 
            && (_Sink_networkAddr == thisnetAddr)
            && ((_Sink_frameType == _type_N_answer_noACK) 
            || (_Sink_frameType == _type_N_answer_ACK)))
        {
          node_batt[addrCurrentnode - startOffset]         = _Sink_batt8bit;
          node_moisture[addrCurrentnode - startOffset]     = _Sink_moisture16bit;
          node_temperature[addrCurrentnode - startOffset]  = _Sink_temperature16bit;
          if (nodetoACK == 1) {
            internalState = 5;  // ACK
          } else {
            internalState = 9;  // non ACK
          }
          getPolling = true;
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT, batt, moisture, temperature");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.print(_Sink_frameType); Serial.print(", ");
          Serial.print(_Sink_batt8bit); Serial.print(", ");
          Serial.print(_Sink_moisture16bit); Serial.print(", ");
          Serial.println(_Sink_temperature16bit);
        }
        
        // join relate
        else if ((result == 2) && (_Sink_addrReceiver == _addr_broadcast) 
                 && (_Sink_addrSender == _addr_joinRequest) 
                 && (_Sink_networkAddr == _netAddr_joinRequest) 
                 && (_Sink_frameType == _type_joinRequest))
        {
          for (int i = 0; i <= joinRequestEUI_FIFO.size(); i++)
          { // check duplicate
            if (!(_Sink_EUI == joinRequestEUI_FIFO[i]))
            { // not duplicate
              joinRequestEUI_FIFO.unshift(_Sink_EUI);
              if (joinRequest < 255) joinRequest += 1;
              Serial.print("add joinRequestEUI : ");
              _EUI64_print(joinRequestEUI_FIFO.last());
              break;
            }
          }
        }
        // netAddr type
        else if ((result == 1) 
                 && ((_Sink_frameType == _type_ask_netAddr) 
                 || (_Sink_frameType == _type_declare_netAddr))) 
        {
          timeStamp_netAddrnegotiate = millis();
          netAddrNegotiate = _Sink_frameType;
          ComnetAddrNegotiate = ComBuffer;
        }
        
        if (getPolling == false)
        {
          Serial.println("Decode > non polling");
          if (nodetoACK == 0) { // no ACK
            internalState = 7;
          } else { // ACK
            internalState = 4;
          }
          break;
        }
        break;
      }
    case 9:  // receive data from node (not reach maxRE)
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println("receive data");
        tonextNode = 1;

        call_printData(addrCurrentnode - startOffset, 1); // success
        //relay_moisture();
        
        internalState = 2;
        break;
      }
    case 10:  // reach maxRE or timeout
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println("reach maxRE or timeout");
        tonextNode = 1;

        call_printData(addrCurrentnode - startOffset, 0); // not success
        //relay_moisture();
        
        internalState = 2;
        break;
      }
    case 20:  // send join accept
      {
        Serial.println("S: " + (String)internalState);

        Serial.print("joinRequestEUI : ");
        _EUI64_print(joinRequestEUI_FIFO.last());
        
        // check whether to accept new node or not
        bool accept = false;
        int size_toAcceptEUI_FIFO = toAcceptEUI_FIFO.size();
        //int size_joinRequestEUI_FIFO = joinRequestEUI_FIFO.size();

        // check if joinRequestEUI == toAcceptEUI
        for (int i = (size_toAcceptEUI_FIFO - 1); i >= 0; i--)
        {
          if (toAcceptEUI_FIFO[i] == joinRequestEUI_FIFO.last())
          {
            accept = true;
            break;
          }
        }
        
        // check if already accept
        for (int i = 32; i <= 254; i++)
        {
          if (joinRequestEUI_FIFO.last() == node_EUI[i - startOffset])
          { // already accept
            Serial.println("Already accept, send accept again");
            // accept node again
            ClearAddr_bitmask(i, Addr_bitmask);
            accept = true;
            break;
          }
        }

        // if not accept, return
        if (accept == false)
        {
          internalState = 1;
          Serial.println("Not accept");
          joinRequestEUI_FIFO.pop();
          if (joinRequest > 0) joinRequest = joinRequest - 1;
          break;
        }

        // check if we accept new node, do we have addr available
        bool full = false;
        for (int i = 32; i <= 254; i++)  // 32 - 254
        { //TestBit return addr(i) that have batmask == 1
          if (!(TestBit (i, Addr_bitmask))) {
            addrToassign = i;
            break;  // break for loop
          }
          if (i == 254) { // full
            full = true;
            break;
          }
        }
        if (full == true)
        { // if full remove join request
          Serial.println("Address full");
          joinRequestEUI_FIFO.pop();
          internalState = 1;
          break;
        }

        // send the accept frame
        Serial.println("Accept");
        frameEncoder_EUI (payloadString,
                          payloadCharArray_EUI,
                          addrToassign,
                          _addr_masterAddr,
                          thisnetAddr,
                          _type_joinAccept,
                          joinRequestEUI_FIFO.last()); // joinRequest EUI joinRequestEUI_FIFO.last()
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray_EUI)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
          timeStamp = millis();
        }
        internalState = 21;
        break;
      }
    case 21:  // wait for node ack
      {
        Serial.println("S: " + (String)internalState);
        if (millis() - timeStamp >= acktimeOut) {
          // timeout reach
          joinRequestEUI_FIFO.pop();
          if (joinRequest > 0) joinRequest = joinRequest - 1;
          internalState = 2;
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL)
        {
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          timeStamp = millis();
          int result = frameDecoder (TestString,
                                    _Sink_addrReceiver,
                                    _Sink_addrSender,
                                    _Sink_networkAddr,
                                    _Sink_frameType,
                                    _Sink_batt8bit,
                                    _Sink_moisture16bit,
                                    _Sink_temperature16bit,
                                    _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          if ((result == 2) 
              && (_Sink_addrReceiver == _addr_masterAddr)
              && (_Sink_addrSender == addrToassign)
              && (_Sink_frameType == _type_join_N_ACK))
          { // node ack
            SetAddr_bitmask(addrToassign, Addr_bitmask);
            Serial.println("add node addr : " + (String)addrToassign);
            node_EUI[addrToassign - startOffset] = joinRequestEUI_FIFO.pop();
            if (joinRequest > 0) joinRequest = joinRequest - 1;
            internalState = 22;
          }
          
          // not ack frame, it node joinRequest
          else if ((result == 2) 
                   && (_Sink_addrReceiver == _addr_broadcast) 
                   && (_Sink_addrSender == _addr_joinRequest) 
                   && (_Sink_networkAddr == _netAddr_joinRequest) 
                   && (_Sink_frameType == _type_joinRequest))
          {
            for (int i = 0; i <= joinRequestEUI_FIFO.size(); i++)
            { // check duplicate
              if (!(_Sink_EUI == joinRequestEUI_FIFO[i]))
              { // not duplicate
                joinRequestEUI_FIFO.unshift(_Sink_EUI);
                if (joinRequest < 255) joinRequest += 1;
                Serial.print("add joinRequestEUI : ");
                _EUI64_print(joinRequestEUI_FIFO.last());
                break;
              }
            }
          }
        }
        break;
      }
    case 22: // ack back to node
      {
        Serial.println("S: " + (String)internalState);

        frameEncoder (payloadString,
                      payloadCharArray,
                      addrToassign,
                      _addr_masterAddr,
                      thisnetAddr,
                      _type_join_M_ACK);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        if (startMainloop == 3) {
          internalState = 2;
        } else internalState = 1;
        break;
      }
    case 25:  // netAddr negotiate
      {
        Serial.println("S: " + (String)internalState);

        if ((millis() - timeStamp_netAddrnegotiate) >= netAddr_answer_timeOut) {
          Serial.print("timeout to answer netAddr negotiate");
          netAddrNegotiate = 0;
          internalState = 2;
          break;
        }

        int num = ComnetAddrNegotiate.indexOf(':');
        String TestString = ComnetAddrNegotiate;
        TestString.remove(0, ++num);
        timeStamp = millis();
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        if (result == 1)
        {
          uint8_t addrSender, frameT;
          // ask netAddr
          if ((_Sink_frameType == _type_ask_netAddr) 
              && (_Sink_addrSender == _addr_declare_netAddr))
          {
            addrSender = 2;
            frameT = _type_answer_netAddr;
          }
          // netAddr declaration
          if ((_Sink_frameType == _type_declare_netAddr) 
              && (_Sink_addrSender == _addr_declare_netAddr))
          {
            addrSender = 4;
            frameT = _type_denial_netAddr;
          }

          frameEncoder (payloadString,
                        payloadCharArray_netAddrnegotiate,
                        _addr_broadcast,
                        addrSender,
                        thisnetAddr,
                        frameT);
          RakLoRa.rk_initP2P_mode(P2P_SENDER);
          if (RakLoRa.rk_sendP2PData(payloadCharArray_netAddrnegotiate)) {
            RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
          }
          netAddrNegotiate = 0;
          internalState = 2;
        }
        else {
          netAddrNegotiate = 0;
          internalState = 2;
        }
        break;
      }
    case 26:  // send remove node
      {
        Serial.println("S: " + (String)internalState);
        addrToRemove = 0;
        for (int i = 32; i <= 254; i++)
        { // check are node to remove exist
          if (TestBit (i, toRemove_bitmask)) {
            addrToRemove = i;
            break;  // break for loop
          }
        }
        if (addrToRemove == 0) {
          Serial.println("toRemove_empty");
          nodeRemove_flag = 0;
          internalState = 2;
          break;
        }

        if ((TestBit (addrToRemove, Addr_bitmask) == 1)
            && (node_EUI[addrToRemove - startOffset] != 0))
          { // send node remove to node
            frameEncoder_EUI (payloadString,
                              payloadCharArray_EUI,
                              addrToRemove,
                              _addr_masterAddr,
                              thisnetAddr,
                              _type_nodeRemove,
                              node_EUI[addrToRemove - startOffset]); // joinRequest EUI joinRequestEUI_FIFO.last()
            RakLoRa.rk_initP2P_mode(P2P_SENDER);
            if (RakLoRa.rk_sendP2PData(payloadCharArray_EUI)) {
              RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
              timeStamp = millis();
            }
            internalState = 27;
            break;
          }
        else {
          Serial.println("noAddr_Remove");
          nodeRemove_flag = 0;
          internalState = 2;
        }
        break;
      }
    case 27:  // wait for node remove ACK (from node)
      {
        Serial.println("S: " + (String)internalState);
        if (millis() - timeStamp >= acktimeOut) {
          // timeout reach abandon node remove (clear flag)
          nodeRemove_flag = 0;
          Serial.print("node remove : not respond");
          internalState = 2;
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL)
        {
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          timeStamp = millis();
          int result = frameDecoder (TestString,
                                    _Sink_addrReceiver,
                                    _Sink_addrSender,
                                    _Sink_networkAddr,
                                    _Sink_frameType,
                                    _Sink_batt8bit,
                                    _Sink_moisture16bit,
                                    _Sink_temperature16bit,
                                    _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          if ((result == 4) 
              && (_Sink_addrReceiver == _addr_masterAddr)
              && (_Sink_addrSender == addrToRemove)
              && (_Sink_frameType == _type_remove_N_ACK))
          {
            // send node remove ACK
            frameEncoder (payloadString,
                          payloadCharArray,
                          addrToRemove,
                          _addr_masterAddr,
                          thisnetAddr,
                          _type_remove_M_ACK);
            RakLoRa.rk_initP2P_mode(P2P_SENDER);
            if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
              RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
            }
            // remove it Addr_bitmask
            ClearAddr_bitmask(addrToRemove, toRemove_bitmask);
            ClearAddr_bitmask(addrToRemove, Addr_bitmask);

            // find and remove toAcceptEUI
            int size_toAcceptEUI_FIFO = toAcceptEUI_FIFO.size();

            // find it EUI
            int i;
            uint64_t toRemoveEUI = 0;
            for (i = (size_toAcceptEUI_FIFO - 1); i >= 0; i--)
            {
              if (toAcceptEUI_FIFO[i] == node_EUI[addrToRemove - startOffset])
              {
                toRemoveEUI = toAcceptEUI_FIFO[i];
                Serial.print("found EUI : ");
                _EUI64_print(toAcceptEUI_FIFO[i]);
                break;
              }
            }
            if (toRemoveEUI != 0)
            { 
              // remove from toAcceptEUI
              for (int j = size_toAcceptEUI_FIFO; j >= 0; j--)
              {
                if (toRemoveEUI == toAcceptEUI_FIFO.first()) {
                  toAcceptEUI_FIFO.shift();
                  toAcceptEUI_FIFO.push(0);
                }
                else {
                  uint64_t tem = toAcceptEUI_FIFO.shift();
                  toAcceptEUI_FIFO.push(tem);
                }
              }
            }

            // remove node_EUI
            Serial.println("remove node addr : " + (String)addrToRemove);
            node_EUI[addrToRemove - startOffset] = 0;
            nodeRemove_flag = 0;
            internalState = 2;
          }
          break;
        }
        break;
      }
    case 30:  // start netAddr negotiate (asking)
      {
        Serial.println("S: " + (String)internalState);

        uint32_t toset_freq;
        if (CH_Sweeping == 14) // jump to state 32
        {
          // stop sweep, move to next state
          internalState = 32;
          break;
        }
        Serial.println("CH_Sweeping : " + (String)CH_Sweeping);

        // sweeping between 14 channels
        if (CH_Sweeping > 0)
        { 
          // sweeping, change CH_FREQ to next Channel
          toset_freq = chTofreq(CH_Sweeping);
        }
        else toset_freq = CH_FREQ;
        
        if (toset_freq != CH_FREQ)
        { // change CH_FREQ to next Channel
          CH_FREQ = toset_freq;
          if (!RakLoRa.rk_initP2P((String)CH_FREQ, SF, BW, CR, PRlen, TX_Power)) {
            String _Buffer;
            do { _Buffer = RakLoRa.rk_recvP2PData();
            } while (_Buffer.indexOf("OK") == -1); //.indexOf return -1 when not found
            Serial.println(_Buffer);
          }
        }

        // encode and sent
        frameEncoder (payloadString,
                      payloadCharArray_netAddrnegotiate,
                      _addr_broadcast,
                      _addr_ask_netAddr,
                      _netAddr_ask,
                      _type_ask_netAddr);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray_netAddrnegotiate)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        timeStamp_netAddrnegotiate = millis();
        internalState = 31;
        break;
      }
    case 31:  // waiting and decode netAddr negotiate
      {
        //Serial.println("S: " + (String)internalState);

        // timeout
        if ((millis() - timeStamp_netAddrnegotiate >= netAddr_negotiate_timeOut))
        {
          CH_Sweeping += 1;
          internalState = 30;
          break;  
        }

        // receive
        ComnetAddrNegotiate = RakLoRa.rk_recvP2PData();
        if (ComnetAddrNegotiate != NULL) 
        {
          Serial.println(ComnetAddrNegotiate);
          int num = ComnetAddrNegotiate.indexOf(':');
          String TestString = ComnetAddrNegotiate;
          TestString.remove(0, ++num);
          int result = frameDecoder (TestString,
                                     _Sink_addrReceiver,
                                     _Sink_addrSender,
                                     _Sink_networkAddr,
                                     _Sink_frameType,
                                     _Sink_batt8bit,
                                     _Sink_moisture16bit,
                                     _Sink_temperature16bit,
                                     _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          // add netAddr to list
          if ((result == 1) 
              && (_Sink_addrReceiver == _addr_broadcast)
              && ((_Sink_addrSender == _addr_answer_netAddr) 
              || (_Sink_addrSender == _addr_declare_netAddr) 
              || (_Sink_addrSender == _addr_denial_netAddr))
              && ((_Sink_frameType == _type_answer_netAddr) 
              || (_Sink_frameType == _type_declare_netAddr) 
              || (_Sink_frameType == _type_denial_netAddr)))
          {
            SetAddr_bitmask(_Sink_networkAddr, netAddr_bitmask);
            Serial.println("add netAddr to list : " + (String)_Sink_networkAddr);
            break;
          }

          //same ask state, skip to calculate netAddr and send netAddr declaration
          else if ((result == 1) 
                   && (_Sink_addrSender == _addr_ask_netAddr) 
                   && (_Sink_frameType == _type_ask_netAddr))
          {
            internalState = 32;
            break; 
          }

          // node answer with EUI
          else if ((result == 1) 
                   && (_Sink_addrSender == _addr_answer_netAddr) 
                   && (_Sink_frameType == _type_N_answer_netAddr))
          {
            int size_toAcceptEUI_FIFO = toAcceptEUI_FIFO.size();

            // check if joinRequestEUI == _Sink_EUI
            for (int i = (size_toAcceptEUI_FIFO - 1); i >= 0; i--)
            {
              if (toAcceptEUI_FIFO[i] == _Sink_EUI)
              { // if _Sink_EUI == toAcceptEUI than declaration the same netAddr
                Serial.println("EUI match, declare the same");
                for (int i = (_Sink_networkAddr - 1); i >= 0; i--) {
                  SetAddr_bitmask(i, netAddr_bitmask);
                }
                ClearAddr_bitmask(_Sink_networkAddr,netAddr_bitmask);
                internalState = 32;
                break;
              }
            }
          }
        }
        break;
      }
    case 32:  // calculate netAddr send declaration
      {
        Serial.println("S: " + (String)internalState);

        // calculate
        uint8_t netAddrToAssign = 255;
        uint32_t toset_freq;
        for (int i = 32; i <= 254; i++)  // 32 - 254
        {
          if (!(TestBit (i, netAddr_bitmask))) {  //TestBit return addr(i) that have batmask == 1
            netAddrToAssign = i;
            toset_freq = chTofreq((netAddrToAssign - startOffset) % totalChannels);
            break;
          }
          if (i == 254) { // none left use netAddr 255
            netAddrToAssign = 255;
            toset_freq = chTofreq((255 - startOffset) % totalChannels);
            break;
          }
        }

        // change channel
        if (toset_freq != CH_FREQ)
        {
          CH_FREQ = toset_freq;
          if (!RakLoRa.rk_initP2P((String)CH_FREQ, SF, BW, CR, PRlen, TX_Power)) {
            String _Buffer;
            do { _Buffer = RakLoRa.rk_recvP2PData();
            } while (_Buffer.indexOf("OK") == -1); //.indexOf return -1 when not found
            Serial.println(_Buffer);
          }
        }

        // send declaration
        frameEncoder (payloadString,
                      payloadCharArray_netAddrnegotiate,
                      _addr_broadcast,
                      _addr_declare_netAddr,
                      netAddrToAssign,
                      _type_declare_netAddr);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray_netAddrnegotiate)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        timeStamp_netAddrnegotiate = millis();
        internalState = 33;
        break;
      }
    case 33:  // waiting and decode netAddr negotiate (2)
      {
        //Serial.println("S: " + (String)internalState);

        // receive
        ComnetAddrNegotiate = RakLoRa.rk_recvP2PData();
        if (ComnetAddrNegotiate != NULL) 
        {
          Serial.println(ComnetAddrNegotiate);
          int num = ComnetAddrNegotiate.indexOf(':');
          String TestString = ComnetAddrNegotiate;
          TestString.remove(0, ++num);
          int result = frameDecoder (TestString,
                                     _Sink_addrReceiver,
                                     _Sink_addrSender,
                                     _Sink_networkAddr,
                                     _Sink_frameType,
                                     _Sink_batt8bit,
                                     _Sink_moisture16bit,
                                     _Sink_temperature16bit,
                                     _Sink_EUI);
          if ((result == 1) 
              && ((_Sink_addrSender == _addr_answer_netAddr) 
              || (_Sink_addrSender == _addr_declare_netAddr) 
              || (_Sink_addrSender == _addr_denial_netAddr)) 
              && ((_Sink_frameType == _type_answer_netAddr) 
              || (_Sink_frameType == _type_declare_netAddr) 
              || (_Sink_frameType == _type_denial_netAddr)))
          { // add netAddr to list
            SetAddr_bitmask(_Sink_networkAddr, netAddr_bitmask);
            Serial.println("add netAddr to list : " + (String)_Sink_networkAddr);
          }
        }

        // timeout, get netAddr, check CH_FREQ
        bool success = false;
        if ((millis() - timeStamp_netAddrnegotiate >= netAddr_negotiate_timeOut))
        {
          int toAssign;
          for (int i = 32; i <= 254; i++)  // 32 - 254
          {
            if (!(TestBit (i, netAddr_bitmask))) {  //TestBit return addr(i) that have batmask == 1
              toAssign = i;
              break;
            }
            if (i == 254) { // none left use netAddr 255
              toAssign = 255;
              break;
            }
          }
          
          // change channel
          int toset_freq = chTofreq((toAssign - startOffset) % totalChannels);
          if (toset_freq != CH_FREQ)
          {
            internalState = 32;
            break;
          }
          else 
          {
            thisnetAddr = toAssign;
            success = true;
          }
        }
        // success get netAddr 
        if (success == true)
        {
          netAddrNegotiate_success = 1;
          internalState = 1;
          timeStamp_loop = millis();
          Serial.println("Success netAddr : " + (String)thisnetAddr);
          Serial.print("Start wait : ");
          Serial.println(T_waitforJoin);
          break;
        }
        break;
      }
    default:
      {
        //Serial.println("S: default (charge to 1)");
        internalState = 1;
        break;
      }
  }

  return internalState;
}

// finite state machine for node
int P2PpollingClass::nodePolling ()
{
  // _Sink_ variable, use for frame decode
  uint8_t     _Sink_addrReceiver      = 0;
  uint8_t     _Sink_addrSender        = 0;
  uint8_t     _Sink_networkAddr       = 0;
  uint8_t     _Sink_frameType         = 0;
  uint8_t     _Sink_batt8bit          = 0;
  uint16_t    _Sink_moisture16bit     = 0;
  uint16_t    _Sink_temperature16bit  = 0;
  uint64_t    _Sink_EUI               = 0;
  
  switch (internalState)
  {
    case 1: // wait (interval), read moisture
      {
        //Serial.println("S: " + (String)internalState);
        if (isJoin == 0) {
          internalState = 20;
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) {
          internalState = 2;
          numReAck = 0;
          numRetry = 0;
        }

        // call outside function set with setXXXfunc method
        call_readBatt(batt8bit);
        call_readTemp(temperature16bit);
        call_readMoisture(moisture16bit);
        break;
      }
    case 2: // Decode
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println(ComBuffer);
        int num = ComBuffer.indexOf(':');
        String TestString = ComBuffer;
        TestString.remove(0, ++num);
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
        Serial.print(_Sink_addrReceiver); Serial.print(", ");
        Serial.print(_Sink_addrSender); Serial.print(", ");
        Serial.print(_Sink_networkAddr); Serial.print(", ");
        Serial.println(_Sink_frameType);
        
        // check for master polling
        if ((result == 3) 
            && (_Sink_addrReceiver == thisAddr) 
            && (_Sink_addrSender == mAddr) 
            && (_Sink_networkAddr == thisnetAddr))
        {
          if (_Sink_frameType == 2)        {  // ACK
            nodetoACK = 1;
            tosend_frameType = _type_N_answer_ACK;
          } else if (_Sink_frameType == 1) {  //  no ACK
            nodetoACK = 0;
            tosend_frameType = _type_N_answer_noACK;
          } else {  // unknow -> go back to state 1
            internalState = 1;
            break;
          }
          internalState = 3;
        }

        // master declare same netAddr again, join again
        else if ((result == 1) 
                 && (isJoin == 1) 
                 && (_Sink_addrSender == _addr_declare_netAddr)
                 && (_Sink_frameType == _type_declare_netAddr) 
                 && (_Sink_networkAddr == thisnetAddr))
        {
          internalState = 20;
          break;
        }

        // master remove this node
        else if ((result == 4) 
                 && (_Sink_addrReceiver == thisAddr) 
                 && (_Sink_addrSender == mAddr) 
                 && (_Sink_networkAddr == thisnetAddr)
                 && (_Sink_frameType == _type_nodeRemove) 
                 && (_Sink_EUI == thisNodeEUI))
        {
          internalState = 26;
          ComRemove = ComBuffer;
          break;
        }

        // we not ready to doing this yet (node answer netAddr negotiate)

        // netAddr negotiate (ask)
        //else if ((result == 1) 
        //         && (isJoin == 1) 
        //         && (_Sink_frameType == _type_ask_netAddr))
        //{
        //  internalState = 25;
        //  timeStamp_netAddrnegotiate = millis();
        //  ComnetAddrNegotiate = ComBuffer;
        //  break;
        //}

        else 
        {
          internalState = 1;
          Serial.println("addrReceiver not match");
        }
        break;
      }
    case 3: // Encode
      {
        Serial.println("S: " + (String)internalState);
        bool arePLok = false;
        arePLok = frameEncoder_Payload (payloadString, 
                                        payloadCharArray,  
                                        mAddr, 
                                        thisAddr, 
                                        thisnetAddr, 
                                        tosend_frameType, 
                                        batt8bit, 
                                        moisture16bit, 
                                        temperature16bit);
        if (arePLok != true) {
          internalState = 1;
          Serial.print("payload ERROR");
          break;
        }
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
          tog = !tog;
          if (ExLED != -1) {
            digitalWrite(ExLED, tog);
          }
        }
        if (nodetoACK == 1) {
          internalState = 4;  // ACK type
        }
        else internalState = 7; // no ACK type
        timeStamp = millis();
        break;
      }
    case 4: // ACK type
      {
        Serial.println("S: " + (String)internalState);
        if (numRetry >= maxRE) {
          internalState = 9; // reach maxRE (no ACK)
          break;
        }
        if ((numRetry > 0) && (millis() - timeStamp >= acktimeOut)) {
          internalState = 8; // timeout reach (Master didn't ask again, get ACK)
          break;
        }
        if (millis() - timeStamp >= acktimeOut) {
          internalState = 6; // timeout reach (master not ACK) to retry
          break;
        }
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) {
          timeStamp = millis();
          internalState = 5;  // timeout and maxRE not reach, to Decode
        }
        break;
      }
    case 5: // Decode ACK
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println(ComBuffer);
        int num = ComBuffer.indexOf(':');
        String TestString = ComBuffer;
        TestString.remove(0, ++num);
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
        Serial.print(_Sink_addrReceiver); Serial.print(", ");
        Serial.print(_Sink_addrSender); Serial.print(", ");
        Serial.print(_Sink_networkAddr); Serial.print(", ");
        Serial.print(_Sink_frameType); Serial.print(", ");
        Serial.print(_Sink_batt8bit); Serial.print(", ");
        Serial.print(_Sink_moisture16bit); Serial.print(", ");
        Serial.println(_Sink_temperature16bit);

        // check master ACK
        if ((result == 3) && (_Sink_addrReceiver == thisAddr)) {
          // receive ACK
          if (_Sink_frameType == _type_M_ACK) {
            internalState = 8;
          } else if (_Sink_frameType == _type_M_ask_N_ACK) {
            internalState = 6;  // send retry
          } else {
            // do something with unknow payload
            internalState = 4;
          }
        }
        else {
          internalState = 4;
          Serial.println("addrReceiver not match");
          break;
        }
        break;
      }
    case 6: // retry
      {
        Serial.println("S: " + (String)internalState);
        bool arePLok = false;
        arePLok = frameEncoder_Payload (payloadString, 
                                        payloadCharArray,  
                                        mAddr, 
                                        thisAddr, 
                                        thisnetAddr, 
                                        tosend_frameType, 
                                        batt8bit, 
                                        moisture16bit, 
                                        temperature16bit);
        if (arePLok != true) {
          internalState = 1;
          Serial.print("payload ERROR");
          break;
        }
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
          timeStamp = millis();
        }
        numRetry += 1;
        internalState = 4;
        break;
      }
    case 7: // no ACK type
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println("no ACK type");
        internalState = 1;
        break;
      }
    case 8:  // receive data from node (not reach maxRE)
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println("receive ACK");
        internalState = 1;
        break;
      }
    case 9:  // reach maxRE or timeout
      {
        //Serial.println("S: " + (String)internalState);
        Serial.println("reach maxRE or timeout");
        internalState = 1;
        break;
      }
    case 20:  // joining
      {
        Serial.println("S: " + (String)internalState);

        if (isJoin == 0)
        { // join again or not
          uint32_t toset_freq;
          toset_freq = chTofreq(CH_Sweeping);

          // sweeping between 14 channels
          if (toset_freq != CH_FREQ)
          { // sweeping, change CH_FREQ to next Channel
            CH_FREQ = toset_freq;
            //Serial.println("CH_Sweeping : " + (String)CH_Sweeping);
            RakLoRa.rk_initP2P((String)CH_FREQ, SF, BW, CR, PRlen, TX_Power);
            String _Buffer;
            do { _Buffer = RakLoRa.rk_recvP2PData();
            } while (_Buffer.indexOf("OK") == -1); //.indexOf return -1 when not found
            Serial.println(_Buffer);
          }
          Serial.println("CH_Sweeping : " + (String)CH_Sweeping);
        }
        frameEncoder_EUI (payloadString,
                          payloadCharArray_EUI,
                          _addr_broadcast,
                          _addr_joinRequest,
                          _netAddr_joinRequest,
                          _type_joinRequest,
                          thisNodeEUI);
        RakLoRa.rk_initP2P_mode(P2P_SENDER);
        if (RakLoRa.rk_sendP2PData(payloadCharArray_EUI)) {
          RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
        }
        timeStamp_join_request = millis();
        internalState = 21;
        break;
      }
    case 21:  // decode accept
      {
        //Serial.println("S: " + (String)internalState);

        if ((millis() - timeStamp_join_request >= CH_Sweep_timeOut))
        {
          CH_Sweeping += 1;
          if (CH_Sweeping == 14) CH_Sweeping = 0;
          internalState = 20;
          break;  
        }

        int result;
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) 
        {
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          timeStamp = millis();
          result = frameDecoder (TestString,
                                 _Sink_addrReceiver,
                                 _Sink_addrSender,
                                 _Sink_networkAddr,
                                 _Sink_frameType,
                                 _Sink_batt8bit,
                                 _Sink_moisture16bit,
                                 _Sink_temperature16bit,
                                 _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);
        
          if ((result == 2) && (_Sink_frameType == _type_joinAccept))
          {
            thisAddr = _Sink_addrReceiver;
            mAddr = _Sink_addrSender;
            thisnetAddr = _Sink_networkAddr;
            frameEncoder (payloadString,
                          payloadCharArray,
                          mAddr,
                          thisAddr,
                          thisnetAddr,
                          _type_join_N_ACK);
            RakLoRa.rk_initP2P_mode(P2P_SENDER);
            if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
              RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
              timeStamp = millis();
            }
            internalState = 22;
          }
          else internalState = 21;
        }
        break;
      }
    case 22:  // decode ack (from master)
      {
        //Serial.println("S: " + (String)internalState);
        if (millis() - timeStamp >= acktimeOut) {
          // timeout reach
          internalState = 20;
          break;
        }

        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) 
        {
          timeStamp = millis();
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          frameDecoder (TestString,
                        _Sink_addrReceiver,
                        _Sink_addrSender,
                        _Sink_networkAddr,
                        _Sink_frameType,
                        _Sink_batt8bit,
                        _Sink_moisture16bit,
                        _Sink_temperature16bit,
                        _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          // check ack from master
          if ((_Sink_addrReceiver == thisAddr) 
              && (_Sink_addrSender == mAddr) 
              && (_Sink_networkAddr == thisnetAddr) 
              && (_Sink_frameType == _type_join_M_ACK))
          {
            isJoin = 1;
            internalState = 1;
            Serial.println("Joined");
            break;
          }
        }
        break;
      }
    case 25:  // netAddr negotiate
      {
        Serial.println("S: " + (String)internalState);

        if ((millis() - timeStamp_netAddrnegotiate) >= netAddr_answer_timeOut) {
          Serial.print("timeout to answer netAddr negotiate");
          internalState = 1;
          break;
        }

        int num = ComnetAddrNegotiate.indexOf(':');
        String TestString = ComnetAddrNegotiate;
        TestString.remove(0, ++num);
        timeStamp = millis();
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        if (result == 1)
        {
          // ask netAddr
          if ((_Sink_frameType == _type_ask_netAddr) 
              && (_Sink_addrSender == _addr_ask_netAddr))
          {
            frameEncoder_EUI (payloadString,
                              payloadCharArray_EUI,
                              _addr_broadcast,
                              _addr_answer_netAddr,
                              thisnetAddr,
                              _type_N_answer_netAddr,
                              thisNodeEUI);
            RakLoRa.rk_initP2P_mode(P2P_SENDER);
            if (RakLoRa.rk_sendP2PData(payloadCharArray_EUI)) {
              RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
            }
          }
          Serial.println("answer with thisnetAddr EUI");
          internalState = 1;
          ComnetAddrNegotiate = "";
          break;
        }
        else {
          internalState = 2;
        }
        break;
      }
    case 26:  // node remove
      {
        //Serial.println("S: " + (String)internalState);
        int num = ComRemove.indexOf(':');
        String TestString = ComRemove;
        TestString.remove(0, ++num);
        timeStamp = millis();
        int result = frameDecoder (TestString,
                                   _Sink_addrReceiver,
                                   _Sink_addrSender,
                                   _Sink_networkAddr,
                                   _Sink_frameType,
                                   _Sink_batt8bit,
                                   _Sink_moisture16bit,
                                   _Sink_temperature16bit,
                                   _Sink_EUI);
        if ((result == 4) 
            && (_Sink_addrReceiver == thisAddr) 
            && (_Sink_addrSender == mAddr) 
            && (_Sink_networkAddr == thisnetAddr)
            && (_Sink_frameType == _type_nodeRemove) 
            && (_Sink_EUI == thisNodeEUI))
        {
          Serial.println("Master remove this node");
          frameEncoder (payloadString,
                        payloadCharArray,
                        mAddr,
                        thisAddr,
                        thisnetAddr,
                        _type_remove_N_ACK);
          RakLoRa.rk_initP2P_mode(P2P_SENDER);
          if (RakLoRa.rk_sendP2PData(payloadCharArray)) {
            RakLoRa.rk_initP2P_mode(P2P_RECEIVER);
            timeStamp = millis();
          }
          internalState = 27;
        }
        else {
          internalState = 1;
          break;
        }
        break;
      }
    case 27:  // wait for node remove ACK (from master)
      {
        //Serial.println("S: " + (String)internalState);

        if (millis() - timeStamp >= acktimeOut) {
          // (temp fix) also set isJoin to 0 when node remove fail
          isJoin = 0;
          internalState = 1;
          break;
        }
        
        ComBuffer = RakLoRa.rk_recvP2PData();
        if (ComBuffer != NULL) 
        {
          Serial.println(ComBuffer);
          int num = ComBuffer.indexOf(':');
          String TestString = ComBuffer;
          TestString.remove(0, ++num);
          int result = frameDecoder (TestString,
                                     _Sink_addrReceiver,
                                     _Sink_addrSender,
                                     _Sink_networkAddr,
                                     _Sink_frameType,
                                     _Sink_batt8bit,
                                     _Sink_moisture16bit,
                                     _Sink_temperature16bit,
                                     _Sink_EUI);
          Serial.println("Decode > addrRe, addrSe, netAddr, frameT ");
          Serial.print(_Sink_addrReceiver); Serial.print(", ");
          Serial.print(_Sink_addrSender); Serial.print(", ");
          Serial.print(_Sink_networkAddr); Serial.print(", ");
          Serial.println(_Sink_frameType);

          // check ack from master
          if ((result == 4) 
              && (_Sink_addrReceiver == thisAddr) 
              && (_Sink_addrSender == mAddr) 
              && (_Sink_networkAddr == thisnetAddr)
              && (_Sink_frameType == _type_remove_M_ACK))
          {
            isJoin = 0;
            internalState = 1;
            Serial.println("this node get remove");
            break;
          }
        }
        break;
      }
    default:
      {
        //Serial.println("S: default (charge to 1)");
        internalState = 1;
        break;
      }
  }

  return internalState;
}

// node remove (inform node)
void P2PpollingClass::nodeRemove (uint64_t toRemove_EUI)
{
  // void type, check if remove OK outside of this function
  addrToRemove = EUI_toAddr(toRemove_EUI);
  if (addrToRemove != 255)
  {
    nodeRemove_flag = 1;
    SetAddr_bitmask(addrToRemove + startOffset, toRemove_bitmask);
  }
  else {
    Serial.print("no EUI : ");
    _EUI64_print(toRemove_EUI);
  }
}

// node remove (NOT inform node, just remove from polling list)
void P2PpollingClass::nodeEUIdelete (uint64_t toRemove_EUI)
{
  uint8_t _addrToRemove = EUI_toAddr(toRemove_EUI);
  if (addrToRemove != 255)
  {
    ClearAddr_bitmask(_addrToRemove + startOffset, toRemove_bitmask);
    ClearAddr_bitmask(_addrToRemove + startOffset, Addr_bitmask);
    Serial.println("nodeEUIdelete addr : " + (String)(addrToRemove + startOffset));
    node_EUI[_addrToRemove - startOffset] = 0;
    nodeRemove_flag = 0;
  }
}

// set function for read batt (node)
bool P2PpollingClass::setReadbattfunc (_outSidefunc_readBatt tocallFunc)
{
  // assign function pointer
  call_readBatt = tocallFunc;
  return true;
}

// set function for read moisture (node)
bool P2PpollingClass::setReadmoisfunc (_outSidefunc_readMoisture tocallFunc)
{
  // assign function pointer
  call_readMoisture = tocallFunc;
  return true;
}

// set function for read temperature (node)
bool P2PpollingClass::setReadtempfunc (_outSidefunc_readTemperature tocallFunc)
{
  // assign function pointer
  call_readTemp = tocallFunc;
  return true;
}

// set print node payload (master)
bool P2PpollingClass::setPrintdatafunc (_outSidefunc_printData tocallFunc)
{
  // assign function pointer
  call_printData = tocallFunc;
  return true;
}

uint8_t P2PpollingClass::getBatt (uint8_t _addr)
{
  return node_batt[_addr];
}

uint16_t P2PpollingClass::getMoisture (uint8_t _addr)
{
  return node_moisture[_addr];
}

uint16_t P2PpollingClass::getTemp (uint8_t _addr)
{
  return node_temperature[_addr];
}

uint8_t P2PpollingClass::EUI_toAddr (uint64_t _EUI)
{ // EUI to address (0 - 222) return address
  uint8_t num = 0;
  for (; num <= 222; num++)
  {
    if (_EUI == node_EUI[num]) 
    { // return addr when found
      break;
    }
    if (num == 222) 
    { // return 255 when NOT found
      num = 255;
      break;
    }
  }
  //Serial.print("EUI_toAddr : ");
  //Serial.println(num);
  return num;
}

// private

// moduleInitialize (RAK4200)
void P2PpollingClass::moduleInitialize (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin)
{
    if (numInstance == 0) {
        numInstance += 1;
    }
    else Serial.println("ERROR single instance only");

    SerialLora_RX_PIN = lora_rx_pin;
    SerialLora_TX_PIN = lora_tx_pin;
    LORA_RES_PIN = lora_reset_pin;
    SerialLora.begin(115200, SERIAL_8N1, lora_rx_pin, lora_tx_pin);
    SerialLora.setRxBufferSize(1024);
    pinMode(LORA_RES_PIN, OUTPUT);
    digitalWrite(LORA_RES_PIN, HIGH);
    SerialDebug.println("module initialize");
}

void P2PpollingClass::_EUI64_print (uint64_t _EUI_64bit)
{
  // print 64bit not support, print 2*32bit
  uint32_t upper = (_EUI_64bit & 0xFFFFFFFF00000000) >> 32;
  uint32_t lower = (_EUI_64bit & 0x00000000FFFFFFFF);
  SerialDebug.print(String(upper, HEX));
  SerialDebug.println(String(lower, HEX));
}

bool P2PpollingClass::frameEncoder_Payload (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                                            uint8_t _addrSender, uint8_t _networkAddr, uint8_t _frameType, 
                                            uint8_t _batt8bit, uint16_t _moisture16bit, uint16_t _temperature16bit)
/*
   _payload(8 bytes) = (8 bits)addrReceiver + (8 bits)addrSender + (8 bits)networkAddr
                       +(8 bits)frameType + (8 bits)batt8bit + (12 bits)moisture16bit
                       +(12 bits)temperature16bit
*/
{
  if (_addrSender == 0) return false;  // can't send from broadcast addr

  // more than 3 HEX can store (only 12bits)
  if ((_moisture16bit > 4095) || (_temperature16bit > 4095)) {
    SerialDebug.println("ERROR data > 4095");
    return false;
  }

  // _CharDigit = 17 = 16 digit + 1 String termination
  _stringPl = "";
  if (_addrReceiver < 16) {
    _stringPl = "0";
    _stringPl += String(_addrReceiver, HEX);
  } else _stringPl = String(_addrReceiver, HEX);

  if (_addrSender < 16) _stringPl += "0";
  _stringPl += String(_addrSender, HEX);

  if (_networkAddr < 16) _stringPl += "0";
  _stringPl += String(_networkAddr, HEX);

  if (_frameType < 16) _stringPl += "0";
  _stringPl += String(_frameType, HEX);

  if (_batt8bit < 16) _stringPl += "0";
  _stringPl += String(_batt8bit, HEX);

  if (_moisture16bit < 16) _stringPl += "0";
  if (_moisture16bit < 256) _stringPl += "0";
  _stringPl += String(_moisture16bit, HEX);

  if (_temperature16bit < 16) _stringPl += "0";
  if (_temperature16bit < 256) _stringPl += "0";
  _stringPl += String(_temperature16bit, HEX);

  //Serial.println("_stringPl : " + _stringPl);

  _stringPl.toCharArray(_charArrayPl, 17);  // 16 char + null
  //Serial.println("_charArrayPl : " + (String)_charArrayPl);

  return true;
}

bool P2PpollingClass::frameEncoder_EUI (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                                        uint8_t _addrSender, uint8_t _networkAddr, 
                                        uint8_t _frameType, uint64_t toSend_EUI)
/*
   _payload(12 bytes) = (8 bits)addrReceiver + (8 bits)addrSender + (8 bits)networkAddr
                       +(8 bits)frameType + (64 bits)EUI
*/
{
  if (_addrSender == 0) return false;  // can't send from broadcast addr

  // _CharDigit = 25 = 24 digit + 1 String termination
  _stringPl = "";
  if (_addrReceiver < 16) {
    _stringPl = "0";
    _stringPl += String(_addrReceiver, HEX);
  } else _stringPl = String(_addrReceiver, HEX);

  if (_addrSender < 16) _stringPl += "0";
  _stringPl += String(_addrSender, HEX);

  if (_networkAddr < 16) _stringPl += "0";
  _stringPl += String(_networkAddr, HEX);

  if (_frameType < 16) _stringPl += "0";
  _stringPl += String(_frameType, HEX);

  uint32_t upper = (toSend_EUI & 0xFFFFFFFF00000000) >> 32;
  uint32_t lower = (toSend_EUI & 0x00000000FFFFFFFF);
  _stringPl += String(upper, HEX);
  _stringPl += String(lower, HEX);

  //Serial.println("_stringPl : " + _stringPl);

  _stringPl.toCharArray(_charArrayPl, 25); // 24 char + null
  //Serial.println("_charArrayPl : " + (String)_charArrayPl);

  return true;
}

bool P2PpollingClass::frameEncoder (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                                    uint8_t _addrSender, uint8_t _networkAddr, uint8_t _frameType)
/*
   _payload(4 bytes) = (8 bits)addrReceiver + (8 bits)addrSender + (8 bits)networkAddr
                       +(8 bits)frameType
*/
{
  if (_addrSender == 0) return false;  // can't send from broadcast addr

  // _CharDigit = 9 = 8 digit + 1 String termination
  _stringPl = "";
  if (_addrReceiver < 16) {
    _stringPl = "0";
    _stringPl += String(_addrReceiver, HEX);
  } else _stringPl = String(_addrReceiver, HEX);

  if (_addrSender < 16) _stringPl += "0";
  _stringPl += String(_addrSender, HEX);

  if (_networkAddr < 16) _stringPl += "0";
  _stringPl += String(_networkAddr, HEX);

  if (_frameType < 16) _stringPl += "0";
  _stringPl += String(_frameType, HEX);

  //Serial.println("_stringPl : " + _stringPl);

  _stringPl.toCharArray(_charArrayPl, 9); // 8 char + null
  //Serial.println("_charArrayPl : " + (String)_charArrayPl);
  return true;
}

int P2PpollingClass::frameDecoder (String _payload, uint8_t& _addrReceiver, uint8_t& _addrSender, 
                                   uint8_t& _networkAddr, uint8_t& _frameType, 
                                   uint8_t& _batt8bit, uint16_t& _moisture16bit, 
                                   uint16_t& _temperature16bit, uint64_t& _EUI)
/*
   payload(4-12 bytes) = (8 bits)addrReceiver + (8 bits)addrSender + (8 bits)networkAddr
                         +(8 bits)frameType 
                         (if node answer) + (8 bits)batt8bit + (12 bits)moisture16bit + (12 bits)temperature16bit 
                         (else join relate) + (64 bits)EUI
*/
{
  int toReturn = 0;

  // check _payload format
  int len = _payload.length();
  if ((len < 8) || (len > 24)) 
  {
    Serial.println("wrong format length < 8 or > 24");
    return toReturn;
  }
  for (int i = 0; i <= --len; i++) {  // check digit 0 - 23
    if (!(isHexadecimalDigit(_payload[i]))) 
    {
      Serial.println("wrong format not HEX");
      return toReturn;
    }
  }

  char buff[(_payload.length() + 1)]; //payloadCharArray
  _payload.toCharArray(buff, (_payload.length() + 1));
  if (_payload.length() >= 8)
  { // header
    char addrRe[3] = {buff[0], buff[1]};
    _addrReceiver = hex2int(addrRe);
    //Serial.println("addrRe : " + (String)_addrReceiver);

    char addrSe[3] = {buff[2], buff[3]};
    _addrSender = hex2int(addrSe);
    //Serial.println("addrSe : " + (String)_addrSender);

    char netAddr[3] = {buff[4], buff[5]};
    _networkAddr = hex2int(netAddr);
    //Serial.println("netAddr : " + (String)_networkAddr);

    char frameT[3] = {buff[6], buff[7]};
    _frameType = hex2int(frameT);
    //Serial.println("frameT : " + (String)_frameType);
    //Serial.println("header ok");
  }

  // have EUI
  if (_payload.length() == 24)
  {
    char join_EUI[17] = {buff[8], buff[9], buff[10], buff[11], buff[12], buff[13], 
                         buff[14], buff[15], buff[16], buff[17], buff[18], buff[19], 
                         buff[20], buff[21], buff[22], buff[23]};
    _EUI = hex2int(join_EUI);
    _EUI64_print(_EUI);
  }
  
  // data type (polling)
  if ((_frameType == _type_M_ask_N_noACK) 
      || (_frameType == _type_M_ask_N_ACK) 
      || (_frameType == _type_N_answer_noACK) 
      || (_frameType == _type_N_answer_ACK) 
      || (_frameType == _type_M_ACK))
  {
    char batt[3] = {buff[8], buff[9]};
    _batt8bit = hex2int(batt);
    //Serial.println("batt : " + (String)_batt8bit);

    char moisture[4] = {buff[10], buff[11], buff[12]};
    _moisture16bit = hex2int(moisture);
    //Serial.println("moisture : " + (String)_moisture16bit);

    char temperature[4] = {buff[13], buff[14], buff[15]};
    _temperature16bit = hex2int(temperature);
    //Serial.println("temperature : " + (String)_temperature16bit);
    toReturn = 3;
    //Serial.println("data type");
  }

  // join type
  else if ((_frameType == _type_joinRequest) 
           || (_frameType == _type_joinAccept)
           || (_frameType == _type_join_N_ACK) 
           || (_frameType == _type_join_M_ACK))
  {
    toReturn = 2;
    //Serial.println("join type");
  }

  // netAddr negotiate type
  else if ((_frameType == _type_ask_netAddr) 
           || (_frameType == _type_answer_netAddr) 
           || (_frameType == _type_N_answer_netAddr)
           || (_frameType == _type_declare_netAddr) 
           || (_frameType == _type_denial_netAddr))
  { 
    toReturn = 1;
    //Serial.println("netAddr type");
  }

  // Remove type
  else if ((_frameType == _type_nodeRemove) 
           || (_frameType == _type_remove_N_ACK) 
           || (_frameType == _type_remove_M_ACK))
  {
    toReturn = 4;
    //Serial.println("remove type");
  }
  return toReturn;
}

uint64_t P2PpollingClass::hex2int(char *hex)
// https://stackoverflow.com/questions/10324/convert-a-hexadecimal-string-to-an-integer-efficiently-in-c/39052834#39052834
// assume ascii char (ASCII 127 characters)
{
  uint64_t val = 0;
  while (*hex) {  // it stop when *hex == 0 (string termination)
    // get current character then increment
    char byte = *hex++;
    // transform hex character to the 4bit equivalent number, 
    // using the ascii table indexes
    if (byte >= '0' && byte <= '9') byte = byte - '0';              // number 0 - 9
    else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;    // lowercase a - f
    else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;    // UPPERCASE A - F
    // shift 4 to make space for new digit, and add the 4 bits of the new digit
    val = (val << 4) | (byte & 0xF);
  }
  return val;
}

// set 256 bit bitmask as array of uint32_t (8*32=256 bit)

// Set the bit at the toSet-th position in Addr_bitmask[]
void P2PpollingClass::SetAddr_bitmask (uint8_t toSet, uint32_t Addr_bitmask[])
{
  // Addr_bitmask[index for which block of 32 bit]
  // than bitwise OR it (set 1) with the index of that within the block

  Addr_bitmask[toSet / 32] |= 1 << (toSet % 32);
}

// Clear the bit at the toSet-th position in Addr_bitmask[]
void P2PpollingClass::ClearAddr_bitmask (uint8_t toSet, uint32_t Addr_bitmask[])
{
  // Addr_bitmask[index for which block of 32 bit]
  // than bitwise AND it (set 0) with the index of that bit within the block
  Addr_bitmask[toSet / 32] &= ~(1 << (toSet % 32));
}

int P2PpollingClass::TestBit (uint8_t toTest, uint32_t Addr_bitmask[])
{
  // return 0 or 1, by bitwise AND that bit with 1 and test for != 0
  return ((Addr_bitmask[toTest / 32] & (1 << (toTest % 32))) != 0 );
}

void P2PpollingClass::PrintAddr_bitmask (uint32_t Addr_bitmask[])
{
  // print from MSB (bit 255) to LSB (bit 0)
  for (int i = 255; i >= 0; i--)
  {
    Serial.print(TestBit (i, Addr_bitmask));;
    //Serial.print(" | ");
  }
  Serial.println();
}

uint32_t P2PpollingClass::chTofreq (int ch)
{
  switch (ch)
  {
    case 0: return FREQ_CH0;
    case 1: return FREQ_CH1;
    case 2: return FREQ_CH2;
    case 3: return FREQ_CH3;
    case 4: return FREQ_CH4;
    case 5: return FREQ_CH5;
    case 6: return FREQ_CH6;
    case 7: return FREQ_CH7;
    case 8: return FREQ_CH8;
    case 9: return FREQ_CH9;
    case 10: return FREQ_CH10;
    case 11: return FREQ_CH11;
    case 12: return FREQ_CH12;
    case 13: return FREQ_CH13;
    default: return FREQ_CH0;
  }
}