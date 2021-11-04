// Copyright (c) kaweewatBumrung. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/*
  loraP2Ppolling.h
  part of loraP2Ppolling arduino library v 1.0.0
  write by: kaweewat bumrung
  date 4/11/2021 (dd/mm/yyyy)
  arduino IDE 1.8.15
  arduino ESP32 core 1.0.6

  to use with ESP32 and RAK4200

  hardware from ETT
  http://www.etteam.com/prodIOT/LORA-RAK4200-AS923-MODULE/index.html
  version 1.0.0
*/

#ifndef loraP2Ppolling_h
#define loraP2Ppolling_h

#define fixPayloadDigit             17

#define spreading_factor            8   // SF 8
#define signal_bandwidth            0   // 0 = 125kHz
#define coding_rate                 4   // 4 = 4/8
#define preamble_length             8   // 8 symbol
#define TX_power_dBm                11  // 11 dBm (module can do 20dBm but allow upto 17)

#include "Arduino.h"

// function pointer
typedef void (*_outSidefunc_printData) (uint8_t _addr, int success);
typedef void (*_outSidefunc_readBatt) (uint8_t& _batt);
typedef void (*_outSidefunc_readTemperature) (uint16_t& _temperature);
typedef void (*_outSidefunc_readMoisture) (uint16_t& _moisture);

class P2PpollingClass
{
  public:
    // Constructor, pin
    P2PpollingClass (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin);

    // begin, start normal config 
    // (master = 1, node = 2), (no_ACK = 0, ACK =1), (_SF = 7 - 12), (_power = 5 - 17)
    bool P2Ppolling_begin (int select_master_node, int ACK_mode, int _SF, int _power);

    // add EUI to Accept
    bool addtoAccept_EUI (uint64_t toAccept_EUI);

    // set node EUI
    bool setNode_EUI (uint64_t toSet_EUI);

    // set exled pin
    void setExled (int _Exled);

    // set interval between polling
    bool setPolling_interval (uint32_t _ms);

    // FSM (finite state machine) polling
    int FSM_polling ();

    // node remove
    void nodeRemove (uint64_t toRemove_EUI);

    // force node remove (NOT inform node, just remove from polling list)
    void nodeEUIdelete (uint64_t toRemove_EUI);

    // set function for read batt (node)
    bool setReadbattfunc (_outSidefunc_readBatt tocallFunc);

    // set function for read moisture (node)
    bool setReadmoisfunc (_outSidefunc_readMoisture tocallFunc);

    // set function for read temperature (node)
    bool setReadtempfunc (_outSidefunc_readTemperature tocallFunc);

    // set print node payload (master)
    bool setPrintdatafunc (_outSidefunc_printData tocallFunc);

    // return node batt from address (address already offset)
    uint8_t getBatt (uint8_t _addr);

    // return node moisture from address (address already offset)
    uint16_t getMoisture (uint8_t _addr);

    // return node temperature from address (address already offset)
    uint16_t getTemp (uint8_t _addr);

    // return node address (already offset) from EUI
    uint8_t EUI_toAddr (uint64_t _EUI);

  private:
    // finite state machine for master
    int masterPolling ();

    // finite state machine for node
    int nodePolling ();

    // moduleInitialize (RAK4200)
    void moduleInitialize (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin);

    // _EUI64_print
    void _EUI64_print (uint64_t _EUI_64bit);

    // frameEncoder_Payload (fix 8 byte)
    bool frameEncoder_Payload (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                                uint8_t _addrSender, uint8_t _networkAddr, uint8_t _frameType, 
                                uint8_t _batt8bit, uint16_t _moisture16bit, uint16_t _temperature16bit);

    // frameEncoder_EUI (fix 12 byte)
    bool frameEncoder_EUI (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                            uint8_t _addrSender, uint8_t _networkAddr, 
                            uint8_t _frameType, uint64_t toSend_EUI);

    // frameEncoder (fix 4 byte)
    bool frameEncoder (String& _stringPl , char _charArrayPl[], uint8_t _addrReceiver, 
                        uint8_t _addrSender, uint8_t _networkAddr, uint8_t _frameType);

    // frameDecoder (4 - 12 byte)
    int frameDecoder (String _payload, uint8_t& _addrReceiver, uint8_t& _addrSender, 
                        uint8_t& _networkAddr, uint8_t& _frameType, 
                        uint8_t& _batt8bit, uint16_t& _moisture16bit, 
                        uint16_t& _temperature16bit, uint64_t& _EUI);

    // from hex(char[]) to uint64_t
    uint64_t hex2int(char *hex);

    // set bit to 1
    void SetAddr_bitmask (uint8_t toSet, uint32_t Addr_bitmask[]);

    // set bit to 0
    void ClearAddr_bitmask (uint8_t toSet, uint32_t Addr_bitmask[]);

    // read bit
    int TestBit (uint8_t toTest, uint32_t Addr_bitmask[]);

    // print all bitmask
    void PrintAddr_bitmask (uint32_t Addr_bitmask[]);

    // return frequency (Hz) from channel
    uint32_t chTofreq (int ch);

    // function pointer for calling
    _outSidefunc_printData          call_printData;
    _outSidefunc_readBatt           call_readBatt;
    _outSidefunc_readTemperature    call_readTemp;
    _outSidefunc_readMoisture       call_readMoisture;

    // variable master and node
    int         numInstance = 0;
    int         SerialLora_RX_PIN;
    int         SerialLora_TX_PIN;
    int         LORA_RES_PIN;

    String      payloadString;
    String      ComBuffer;
    String      ComnetAddrNegotiate;
    char        payloadCharArray[fixPayloadDigit];
    char        payloadCharArray_netAddrnegotiate[9];
    char        payloadCharArray_EUI[25];

    uint32_t    CH_FREQ;
    int         SF = spreading_factor;
    int         BW = signal_bandwidth;
    int         CR = coding_rate;
    int         PRlen = preamble_length;
    int         TX_Power = TX_power_dBm;  // max 17

    uint32_t    Addr_bitmask[8] = {0, 0, 0, 0, 0, 0, 0, 0};      // 256 bits in 8 array of uint32_t
    uint32_t    netAddr_bitmask[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // 256 bits in 8 array of uint32_t
    uint32_t    toRemove_bitmask[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // 256 bits in 8 array of uint32_t

    uint8_t     thisnetAddr;

    int         nodetoACK;  // 0 > not ACK, 1 to ACK
    int         set_master_or_node;
    int         maxRE = 3;  // numRetry + numReAck

    uint64_t    node_EUI[223];

    int         internalState;
    uint32_t    timeStamp;
    uint32_t    timeStamp_netAddrnegotiate;
    
    int         joinRequest;
    int         netAddrNegotiate;
    uint8_t     tosend_frameType;
    int         numRetry;
    int         numReAck;
    int         CH_Sweeping;

    // variable master only
    String      ComjoinRequest;
    uint32_t    T_Interval = 10000;
    int         nodeRemove_flag = 0;
    uint8_t     _fix_masterAddr = 16;  // master addr fix at 16

    int         startMainloop;
    uint32_t    timeStamp_loop;
    uint8_t     addrCurrentnode;
    int         tonextNode;
    uint8_t     addrToRemove;
    uint8_t     addrToassign;
    int         netAddrNegotiate_success;

    uint8_t     node_batt[223];         // 0 - 255
    uint16_t    node_moisture[223];     // only 3 hex digit (12 bit)
    uint16_t    node_temperature[223];  // only 3 hex digit (12 bit)

    // variable node only
    String      ComJoining;
    String      ComRemove;
    uint64_t    thisNodeEUI; // EUI 16 HEX digit, 64 bits
    uint8_t     mAddr;
    uint8_t     thisAddr;

    int         passFirst_run;
    uint32_t    timeStamp_join_request;
    int         isJoin;

    uint8_t     batt8bit;
    uint16_t    moisture16bit;    // raw "xxx" for prototype
    uint16_t    temperature16bit; // shift +128 ex 0C->128, -128C->0, -10C->118, 25C->153
    int         ExLED = -1;
    bool        tog = false;
};

#endif