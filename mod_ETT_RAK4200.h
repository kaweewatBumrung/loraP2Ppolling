/*
  mod_ETT_RAK4200.cpp
  for use with loraP2Ppolling
  modify by kaweewat bumrung
  - loraP2P read AT-Command to readStringUntil(10) (/n)
  - add _timeout_2 = 27ms
  date 30/10/2021 (dd/mm/yyyy)

  from rakwireless RAK4200 AT-Command
  https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4270-Module/AT-Command-Manual/
  AT Command 115200 / 8-N-1
  full 255 ASCII characters and than <CR><LF> (/r/n) ASCILL (13,10)
  minimum response of 5 ASCII characters "OK /r/n" -> 'O'+'K'+' '+'/r'+'/n'

  use with hardware from ETT
  http://www.etteam.com/prodIOT/LORA-RAK4200-AS923-MODULE/index.html
  http://www.ett.co.th/prodIOT/cat-LORA-RAK4200.pdf
*/

/*
 * A library for controlling RAK4200 LoRa radio.
 *
 * @Author Eakachai Makarn(modify from RAK811 from Leopold.wang)
 * @Date   15/2/2021
 *
 */

//=================================================================================================
#ifndef RAK4200_h
#define RAK4200_h
//=================================================================================================

//=================================================================================================
#define LoRaWAN       0
#define LoRaP2P       1
//=================================================================================================
#define WAN_OTAA      0
#define WAN_ABP       1
//=================================================================================================
#define P2P_RECEIVER  1
#define P2P_SENDER    2
//=================================================================================================

//=================================================================================================
#include "Arduino.h"
//=================================================================================================

//=================================================================================================
//#define DEBUG_MODE
#define DEBUG_MODE true
//=================================================================================================

//=================================================================================================
class RAK4200
{
  public:

    /*
     * A simplified constructor taking only a Stream ({Software/Hardware}Serial) object.
     * The serial port should already be initialised when initialising this library.
     */
    RAK4200(Stream& serialLora, Stream& serialDebug);

    /* STDB
     * Set the module serial port parameters.Module restart effective
     * UartPort :UART Port number
     * Band : Serial baud rate.Supports baud rate: 9600  19200  38400  57600  115200  230400  460800  921600.
     */
    bool rk_setUARTConfig(int UartPort, int Baud);
    
    /*
     * Gets the firmware version number of the module.
     * Only applies to the firmware that the module programmed for the RAK4200 AT command.
     * AT commands refer to: https://downloads.rakwireless.com/en/LoRa/RAK4200/Application_Notes/Get_Start_with_RAK4200_WisNode-LoRa.pdf
     */
    String rk_getVersion(void);

    /* STDB
     * Check the device statistics.
     */
    String rk_checkDeviceStatus(void);
    
    /* STDB
     * Get the frequency band of the module.
     * This feature request to receive at least 800 bytes buffer size.
     */
    String rk_getLoRaStatus(void);

    /*
     * Let the module enter the ultra low power sleep mode.
     * When the module is in sleep mode, the host can send any character to wake it up.
     * mode :0->wakeup, 1-> sleep
     * When the module is awakened, the event response will automatically return through the serial information.
     */
    bool rk_sleep(int mode);

    /*
     * Reset the module.
     */
    bool rk_reset(void);

    /*
     * This command is used to set the interval time of sending data.
     * This feature can't use on RAK4200,manually close the feature before user APP.
     */
    bool rk_setSendinterval(int value );

    /*
     * Use to change the next send data rate temporary when adr function is off.
     * It will not be save to internal flash.
     * rate : If your Band is EU868 from 0 to 7.
     *        If your Band is US915 from 0 to 4.
     */
    bool rk_setRate(int rate);

    /*
     * Use to change the LoRaWAN class.
     * classMode:0->ClassA,
     *           1->ClassB,
     *           2->ClassC
     * It will be save to internal flash.
     */
    bool rk_setClass(int classMode);  

    /*
     * Use to change the LoRaWAN region.
     * region:0->AS923,
     *        1->AU915,
     *        2->CN470,
     *        3->CN779,
     *        4->EU433,
     *        5->EU868,
     *        6->KR920,
     *        7->IN865,
     *        8->US915,
     *        9->US915_Hybrid,
     * It will be save to internal flash.
     */
    bool rk_setRegion(int region);  

    /*
     * Set the module work mode, the module defaults to LoRaWAN mode..
     * mode  = 0: Set the module to LoRaWAN mode.
     * mode  = 1: Set the module to LoRaP2P mode.
     * This command could cause modual reset,so this function must only be executed once
     */
    bool rk_setWorkingMode(int mode);

    /*
     * Set the activation mode to join the network.And join the network.
     * mode  = 0: join a network using over the air activation..
     * mode  = 1: join a network using personalization.
     * Before using this command, you must call one of the rk_initOTAA or rk_initABP functions
     */
    bool rk_setJoinMode(int mode);

    /*
     * Join the network.
     * timeout: timeout value (unit:s)
     * Before using this command, you must call one of the rk_setJoinMode functions
     */
    bool rk_joinLoRaNetwork(int timeout);

    /* STDB
     * Get the Channels list at current region.
     * This feature request to receive at least 800 bytes buffer size if work at Region US915,AU915 or CN470
     */
    String rk_getChannelList(void);

    /*
     * After joining the network, send the packet at the specified application port.
     * port : The port number.(1-223)
     * datahex : hex value(no space). max 64 byte.
     * This function can only be used in module work in LoRaWAN mode.
     */
    bool rk_sendData( int port, char* datahex);

    /*
     * LoRa data send package type.
     * type : 0->unconfirm, 1->confirm
     * This function can only be used in module work in LoRaWAN mode.
     */
    bool rk_isConfirm(int type);

    /*
     * Send a raw command to the RAK4200 module.
     * //Returns the raw string as received back from the RAK4200.
     * Return true,send OK
     * If the RAK4200 replies with multiple line, only the first line will be returned.
     */
    bool sendRawCommand(String command);
    
    /*
     * Returns the data or event information received by the module.
     *
     */
    String rk_recvData(void);
    //=============================================================================================
    
    //=============================================================================================
    // Start of OTAA Function Group
    //=============================================================================================
    /*
     * Initialize the module parameter, which is the parameter that the module must use when adding the OTAA to the network.
     * devEUI : Device EUI as a HEX string. Example "60C5A8FFFE000001"
     * appEUI : Application EUI as a HEX string. Example "70B3D57EF00047C0"
     * appKEY : Application key as a HEX string. Example "5D833B4696D5E01E2F8DC880E30BA5FE"
     */
    bool rk_initOTAA(String devEUI, String appEUI, String appKEY);
    //=============================================================================================
    // End of OTAA Function Group
    //=============================================================================================
    
    //=============================================================================================
    // Start of ABP Function Group
    //=============================================================================================
    /*
     * Initialize the module parameter, which is the parameter that the module must use when adding the ABP to the network.
     * devADDR : The device address as a HEX string. Example "00112233"
     * nwksKEY : Network Session Key as a HEX string. Example "3432567afde4525e7890cfea234a5821"
     * appsKEY : Application Session Key as a HEX string. Example "a48adfc393a0de458319236537a11d90"
     */
    bool rk_initABP(String devADDR, String nwksKEY, String appsKEY);
    //=============================================================================================
    // End of ABP Function Group
    //=============================================================================================
    
    //=============================================================================================
    // Start of P2P Function Group
    //=============================================================================================
    /*
     * Initialize the required parameters in LoRaP2P mode.
     * You must first switch the module operating mode to LoRaP2P mode
     * FREQ : frequency, default 860000000 range: (860000000 ~1020000000)
     * SF : spread factor, default 7 ( 6-10) more low more fast datarate
     * BW : Band-with, default 0 ( 0:125KHz, 1:250KHz, 2:500KHz)
     * CR : coding Rate, default 1 (1:4/5, 2:4/6, 3:4/7, 4:4/8)
     * PRlen : Preamlen, default 8 (8-65535)
     * PWR : Tx power, default 14 (5-20)
     * User can use this command to build their point to point communication or RFtest command. It will save to flash.
     */
    bool rk_initP2P(String FREQ, int SF, int BW, int CR, int PRlen, int PWR);

    bool rk_initP2P_mode(int mode);
    
    /*
     * This function is used to LoRaP2P send data
     * datahex : hex value ( no space) .
     */
    bool rk_sendP2PData(char* datahex);

    /*
     * Set LoRaP2P Rx continues,module will receive packets with rfconfig until receive the rk_stopRecvP2PData command or rk_reset.
     */
    String rk_recvP2PData(void);
    //=============================================================================================
    // End of P2P Function Group
    //=============================================================================================
    
  private:
    //=============================================================================================
    Stream& _serialLora;                                                                          // Lora interface 
    Stream& _serialDebug;                                                                         // debug interface
    //=============================================================================================

    //=============================================================================================
    // OTAA
    //=============================================================================================
    String _devEUI  = "60C5A8FFFE000001";
    String _appEUI  = "";
    String _appKEY  = "";
    //=============================================================================================
    
    //=============================================================================================
    // ABP
    //=============================================================================================
    String _nwksKEY = "";
    String _appsKEY = "";
    String _devADDR = "00112233";
    //=============================================================================================
};
//=================================================================================================

//=================================================================================================
#endif
//=================================================================================================