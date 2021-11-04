# loraP2Ppolling
This is Arduino library to communicate between device using **lora P2P** (Peer To Peer)

loraP2Ppolling is **NOT** using LoRaWAN [lora vs LoRaWAN][lora vs LoRaWAN link] it only use lora P2P (Peer To Peer) which is physical layer of LoRaWAN. the goal for this library are to create simple communication link from sensors at many nodes to a single device like a point-to-multipoint. <ins>e.g.</ins> read moisture sensors from many nodes into single ESP32 than you can do whatever with that data. 

Why not use LoRaWAN? to use LoRaWAN that mean you have to use one of LoRaWAN network operator who have coverage at your location like the things network or here in thailand we also have [LoRa IoT by CAT][catLoRa link]. but most of the time you will have some [restriction][ttn fair-use link] or subscription cost. In another way you can have your own Private networks but that need LoRaWAN gateway. which price start around 70$ or more for a simple indoor gateway ([TTIG][The Things Indoor Gateway link], [pygate+WiPy][pygate link]). this lead me to think about using just lora P2P for some simple network. but of cause the advantage of LoRaWAN are it gataway that can work with multiple spreading factor and frequency band. but for simple small network that is just collect data from multiple sensors. using less capability lora device such as as normal end device type lora module which are cheaper. and this way you also have more control over your own network. this library also use external library [CircularBuffer][CircularBuffer link]

**Warning :** There are local regulation [กฏ กสทช.][กฏ กสทช. link] here in thailand. you **MUST** check what are status of rule are at the time. and i might not be able to update this library or this document or this warning at that time. always check the rule. what we using is just lora but more commonly we see this type of device using LoRaWAN so i try to understand it in the context of LoRaWAN which here in thailand are usable within 920MHz - 925MHz and for LoRaWAN it use frequency plan AS923. what i understand there 3 rule that this type of device must follow
- **Power limit** in term of e.i.r.p. no more than 50mW which is ~17dBm. (there are more power option but you need license)
- **Duty cycle** with in 1 hour no more than 1%.
- **Dwell Time** no more than 400ms. i'm not sure about what this will applied in what we will doing. dwell time is (what i think) are duration spend in single channel with a specific bandwidth in the FHSS (Frequency Hopping Spread Spectrum) system. in LoRaWAN the hopping part are (again what i think) the fact that node can send frame to gateway in multiple channel because gateway can receive frame from multiple channel with difference spreading factor at the same time. so in what this library aim to do there are frequency switching but once device create/joined network it will not switching between frequency any more. this is main difference of what this library do and what LoRaWAN do. maybe what i want to do is not FHSS any more. and difference rule will applied but if it the same than that simple mean no more than 400ms time on air. which mean you can't use SF11, SF12 because minimum frame length (just bare header) is already longer than that and this applied to LoRaWAN as well [LoRaWAN air time calculator][time on air calculator link].

and  keep in mind that you should check from difference source for what the rule mean. do not just trust me

### How it work in detail

![system_image](/README_assets/polling_crop.png)

The core function that we want are to polling data from each node into to master. here node mean end device and you can think of master as a kind of a gateway to the internet or user. master and node using lora P2P as a communicate link. polling here simple mean you keep asking each node about it sensor data. and we just send that data using lora P2P. but lora frame is just broadcast into air like a normal wireless does. so here in this system i design number of thing.
- Frame format and header to know who send, who receive, and what kind of frame is it. 
- Address, for master it will have a fix address but each node we need to be able to distinguish between each node. so we use EUI (Extended Unique Identifiers). that should be unique to every node which can be found on sticker of RAK4200 module. but it is 64 bits long which is too long. so we will have master assign 8 bits address to each node after node joined master own network and use that address for polling instead. maximum number of node are 223.
- Network address there can be multiple networks working independently. the network itself design to be a star topology with master at the center and many nodes around it. if you want to have more than one network you can try but there are some limitation [Note](#note).
- I choose to mimic the frequency channel ues in LoRaWAN frequency plan AS923. which i choose 9 channel at 923.2, 923.4, 923.6, 923.8, 924, 924.2, 924.4, 924.6 and 924.8MHz. this may change in the future as i look into using frequency outside of LoRaWAN to avoid interference with it.
- Fix bandwidth at 125kHz coding rate of 4/8 and 8 symbol of preamble. you can set the spreading factor (SF) and power but once set it will be fix through the rest of operation.
- system that use difference SF should work without interfere other system in other SF. but i don't have enough device to test how much it can tolerate.

**Warning :** This is simple star topology with master-nodes and as of now specific hardware (ESP32 with RAK4200). it may not be suitable for your use case. (see [Note](#note)) I first write this library intent for personal use. but as it functionality grow i thought i should share this so it might be helpful to someone else and also to practice my skill include write all this up as well. also i am just a beginner in lora/LoRaWAN or in programming in general and this is my first repository. please forgive me if this library/code done something wrong or inefficient to your use case (or if you have trouble read my code or my english skill here are too bad). if you want to verify how it work or how to modify it you can inspect every single line of code here yourself. also before i start making this library i look around for something similar but could not find any maybe there and i just can't find it.

#### Frame format
To send some data with lora we call that data payload. that can only be compose of number of full bytes. we write it to be string of hexadecimal number. we will call payload frame from now. frame can be divide into header and data/EUI

![format_image](/README_assets/frame_format.png)

>Frame = address receiver (8 bits) + address sender (8 bits) + network address + frame type (8 bits) + node data (32 bits) or EUI (64 bits)

what each fields mean
- **address receiver** (addrReceiver) are 8 bits address that every master and node have and it **Only** unique within it own network. for both master and node that not create/joined network it will have unique predefine address. master address are fix but node address are dynamically assign by master when node joined a network using node EUI.
- **address sender** (addrSender) same as address receiver
- **network address** (netAddr) are 8 bits address for every network. when master create network it will assign itself and it own node the same netAddr.
- **frame type** (frameType) are 8 bits data that describe what frame is it. master/node will do difference thing about any frame base on what type of it and what state of master/node at that moment.
- **data** this is the sensors data that master polling from node. fix length at 32 bits and there 3 data mix together 8 bit Batt, 12 bit moisture, 12 bit temperature. you can't change length or number of data and you might want to so see [Note](#note)
- **EUI** are Extended Unique Identifiers we use it to distinguish between each node before it joined a network. and also when need to remove that node as well.

#### Network address negotiate

![netAddr_neg_image](/README_assets/netAddr_neg.png)

Master will create it own network which have it own netAddr in the range of 32 - 254. after power on it will start at first channel (CH0) and send netAddr negotiate frame asking all other master to answer with there own netAddr. it will remember all the receive netAddr and change to next channel after time out and do the same thing again. if it finish sweep across all channel it will than calculate netAddr that is not a duplicate of any other netAddr and than declare that netAddr. if no one denial that netAddr it will claim that netAddr as it own. but if some master denial that netAddr it will calculate new netAddr change channel and declare again. if none of netAddr are available than it will use predefine netAddr of 255.

#### Joining

![join_image](/README_assets/joining.png)

Node will join network and have it own address, netAddr which it will get from master. after power on it will start joining which is sending join request frame at first channel than wait for master join accept if it not receive that within fix duration it will change to next channel and do the same and will loop back to first channel until master send join accept. than it will send ACK back and wait for master ACK. if master not receive node ACK it won't retry and joining set to fail which if node still present node will try to join again later. same thing if node not receive master ACK after it send node ACK it won't retry and just change channel after time out. if master still present it will get another chance later. the join accept frame from master (with node correct EUI) in header node will use addrReceiver in the range of 32 - 254 as well as netAddr as it own address and netAddr.

#### Polling with and without ACK

![polling_image](/README_assets/polling_with_without_ack.png)

Polling can be done with or without acknowledgment (ACK). only joined node can be polling. with ACK mode master send frame asking for data with frameType set to ACK mode. node will send it data and wait for master ACK if node not receive master ACK after set duration it will send data again (retry). the number of retry are fix at 3 that mean it can send up to 4 time. same for the master if it not receive node data after set duration it will ask again (retry) and can also do it up to 4 time. in without ACK mode master send frame asking for data with frameType set to no ACK mode. and it wait for answer within set duration if node didn't answer it won't retry and continue polling next node instead. and for the node it will send data and than don't care because there aren't retry in without ACK mode.

#### Node remove

![node_remove_image](/README_assets/node_remove_pic.png)

Master can remove node which will set it to start doing join request again. first master send node remove frame with node EUI to node that it want to remove. than node will send ACK back and wait for master ACK. if master not receive node ACK after it send node remove master will set node remove as fail.

#### Finite state machine
The core function for both master and node is just finite state machine. it use to describe what the machine (system) that can be in one of a finite number of states at a time. and it can change from one state to another in response to some inputs (state transition). i use it to design the system as well as describe it in the table below. keep in mind that this table will be just a simplify version.

**Master state machine**
<table>
<thead>
  <tr>
    <th>State</th>
    <th>Description</th>
    <th>State transition condition (from.to)</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td>S1</td>
    <td>Start State<br>- between main polling interval<br>- wait for any frame to arrive</td>
    <td>1.30 : when we need to start network address negotiate<br>1.2&nbsp;&nbsp;&nbsp;: when it time to start polling node<br>1.20 : when receive join relate frame<br>1.25 : when receive network address negotiate frame or flag was set<br>1.26 : when we need to remove node</td>
  </tr>
  <tr>
    <td>S2</td>
    <td>Ready to polling<br>- check what address of node to polling<br>- move to next node (address)<br>- check if we need to process other frame<br>(netAddr negotiate, join relate)</td>
    <td>2.1&nbsp;&nbsp;&nbsp;: when no more node to polling (endOfpolling)<br>2.20 : when we need to process join relate frame<br>2.25 : when netAddr negotiate flag was set<br>2.26 : when we need to remove node<br>2.3   : when we begin or continue polling</td>
  </tr>
  <tr>
    <td>S3</td>
    <td>Start polling<br>- encode frame and send payload to node</td>
    <td>3.4   : when finish sending with ACK mode<br>3.7   : when finish sending without ACK mode</td>
  </tr>
  <tr>
    <td>S4</td>
    <td>ACK mode<br>- wait for any frame<br>- check if we reach timeout<br>- check if we reach max retry<br>- check if node receive ACK, didn't retry</td>
    <td>4.10 : when we reach max retry (polling fail)<br>4.9&nbsp;&nbsp;&nbsp;: when node receive ACK (success)<br>4.6&nbsp;&nbsp;&nbsp;: when node not respond, to retry<br>4.8&nbsp;&nbsp;&nbsp;: when we receive any frame</td>
  </tr>
  <tr>
    <td>S5</td>
    <td>Send ACK<br>- send ACK</td>
    <td>5.4&nbsp;&nbsp;&nbsp;: when finish sending</td>
  </tr>
  <tr>
    <td>S6</td>
    <td>Send retry<br>- Send retry (ask again)</td>
    <td>6.4   : when finish sending or run out of retry</td>
  </tr>
  <tr>
    <td>S7</td>
    <td>no ACK mode<br>- wait for any frame<br>- check if we reach timeout</td>
    <td>7.10 : when timeout (polling fail)<br>7.8&nbsp;&nbsp;&nbsp;: when we receive any frame</td>
  </tr>
  <tr>
    <td>S8</td>
    <td>Frame decoder<br>- decode frame and look at what type of frame is it<br>- if it join request frame add to <br>join request list<br>- if netAddr negotiate set flag</td>
    <td>8.5&nbsp;&nbsp;&nbsp;: when it polling with ACK type<br>8.9&nbsp;&nbsp;&nbsp;: when it polling without ACK type<br>8.7&nbsp;&nbsp;&nbsp;: when it non polling and we in without ACK mode<br>8.4&nbsp;&nbsp;&nbsp;: when it non polling and we in with ACK mode</td>
  </tr>
  <tr>
    <td>S9</td>
    <td>Polling success<br>- call function that we set up to call<br>at polling finish and set it as success<br></td>
    <td>9.2&nbsp;&nbsp;&nbsp;: when finish function call</td>
  </tr>
  <tr>
    <td>S10</td>
    <td>Polling fail<br>- call function that we set up to call<br>at polling finish and set it as not success</td>
    <td>10.2 : when finish function call</td>
  </tr>
  <tr>
    <td>S20</td>
    <td>Join accept<br>- check join relate frame that we receive<br>- send accept if it the EUI we want</td>
    <td>20.1 : when it not the EUI we want to accept<br>20.21: when it is the EUI we want to accept<br></td>
  </tr>
  <tr>
    <td>S21</td>
    <td>Wait for node to answer join accept<br>- wait for node answer join accept frame<br>- if receive another join request add to join request list</td>
    <td>21.2 : when timeout (node not answer)<br>21.22: when node answer</td>
  </tr>
  <tr>
    <td>S22</td>
    <td>ACK back when node join<br>- Send ack back at node with netAddr <br>and node address for that node</td>
    <td>22.2 : when finish sending</td>
  </tr>
  <tr>
    <td>S25</td>
    <td>netAddr negotiate<br>- check what to answer that frame<br>- if it asking than answer<br>- if it declare check if conflict and if <br>we need to send denial</td>
    <td>25.2 : when finish</td>
  </tr>
  <tr>
    <td>S26</td>
    <td>Remove node<br>- check if the node to remove exist<br>- if it is than send node remove to node</td>
    <td>26.2 : when node to remove not exist<br>26.27: when we finish sending node remove</td>
  </tr>
  <tr>
    <td>S27</td>
    <td>Wait for node to answer node remove<br>- wait for node to answer<br>- if node answer (ACK back) than remove node from polling list, to accept list<br>- clear node remove flag when finish or timeout</td>
    <td>27.2 : when finish remove node or node not answer</td>
  </tr>
  <tr>
    <td>S30</td>
    <td>Start network address negotiate<br>- start network address negotiate <br>to create network and send asking frame (start sweep)<br>- check if finish sweeping all 9 channel</td>
    <td>30.32: when finish in all channel and no answer<br>30.31: when finish sending</td>
  </tr>
  <tr>
    <td>S31</td>
    <td>Wait for netAddr negotiate frame<br>- wait for netAddr negotiate frame<br>- check if sweep timeout (to next channel)<br>- if receive answer frame add to list</td>
    <td>31.30: when timeout (to next channel)<br>31.32: when receive asking frame skip the rest of sweep</td>
  </tr>
  <tr>
    <td>S32</td>
    <td>Declare netAddr<br>- calculate netAddr from list<br>- send declare netAddr</td>
    <td>32.33: when finish</td>
  </tr>
  <tr>
    <td>S33</td>
    <td>Wait for netAddr negotiate frame<br>- wait for netAddr negotiate frame<br>- if receive answer, declare, denial add to list<br>- if there new netAddr in list calculate netAddr<br>again<br>- if we have new netAddr change channel</td>
    <td>33.32: when we need to chane channel (need to declare again)<br>33.1 : when success</td>
  </tr>
</tbody>
</table>

**Node state machine**
<table>
<thead>
  <tr>
    <th>State</th>
    <th>Description</th>
    <th>State transition condition (from.to)</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td>S1</td>
    <td>Start State<br>- wait for any frame to arrive<br>- call function that we set up to call when wait for master to polling</td>
    <td>1.20 : when we need to start joining<br>1.2&nbsp;&nbsp;&nbsp;: when we receive any frame</td>
  </tr>
  <tr>
    <td>S2</td>
    <td>Decode<br>- decode frame and look at what type of frame is it<br>- if it polling from master is it ACK or no ACK</td>
    <td>2.1   : when receive unknow frame<br>2.3&nbsp;&nbsp;&nbsp;: when receive polling frame<br>2.20 : when receive declare same netAddr (join again)<br>2.26 : when receive this node remove</td>
  </tr>
  <tr>
    <td>S3</td>
    <td>Sending<br>- encode frame and send data to master<br>- toggle exled if set up</td>
    <td>3.1   : when can't encode data (wrong format)<br>3.4   : when finish sending with ACK mode<br>3.7   : when finish sending without ACK mode</td>
  </tr>
  <tr>
    <td>S4</td>
    <td>ACK mode<br>- wait for any frame<br>- check if we reach timeout<br>- check if we reach max retry<br>- check if master receive data, didn't ask again</td>
    <td>4.9   : when reach max retry<br>4.8   : when master receive data, didn't ask again<br>4.6   : when time out, master not ACK (to retry)<br>4.5   : when we receive any frame</td>
  </tr>
  <tr>
    <td>S5</td>
    <td>Decode master ACK<br>- decode frame and look at what type of frame is it<br></td>
    <td>5.8   : when receive master ACK<br>5.6   : when receive master polling (master ask again)<br>5.4   : when receive unknow frame</td>
  </tr>
  <tr>
    <td>S6</td>
    <td>Send retry<br>- Send retry (ask again)</td>
    <td>6.1&nbsp;&nbsp;&nbsp;: when can't encode data (wrong format)<br>6.4&nbsp;&nbsp;&nbsp;: when finish sending</td>
  </tr>
  <tr>
    <td>S7</td>
    <td>no ACK mode<br>- do nothing. just print to serial<br>(because there are no retry)</td>
    <td>7.1&nbsp;&nbsp;&nbsp;: when finish</td>
  </tr>
  <tr>
    <td>S8</td>
    <td>Receive ACK<br>- do nothing. just print to serial</td>
    <td>8.1&nbsp;&nbsp;&nbsp;: when finish</td>
  </tr>
  <tr>
    <td>S9</td>
    <td>Time out or reach max retry<br>- do nothing. just print to serial</td>
    <td>9.1&nbsp;&nbsp;&nbsp;: when finish</td>
  </tr>
  <tr>
    <td>S20</td>
    <td>Start joining<br>- Send join request<br>- start sweep all 9 channel</td>
    <td>20.21: when finish sending</td>
  </tr>
  <tr>
    <td>S21</td>
    <td>Decode accept<br>- wait for any frame<br>- if master send accept, <br>send accept ACK back<br>- if master accept set it own address, netAddr<br>,master address as what master send </td>
    <td>21.20: when timeout to change channel<br>21.22: when receive accept frame from master</td>
  </tr>
  <tr>
    <td>S22</td>
    <td>Decode accept ACK<br>- wait for frame<br>- if master send ACK back, set it self as joined</td>
    <td>22.20: when timeout (start join request again)<br>22.1  : when receive master ACK</td>
  </tr>
  <tr>
    <td>S25</td>
    <td>(NOT in use) Node do netAddr negotiate<br>- as of v1.0.0 not in use</td>
    <td>25.1  : when finish sending (NOT in use)</td>
  </tr>
  <tr>
    <td>S26</td>
    <td>Node remove<br>- check if master remove this node<br>- if master remove this node send ACK back</td>
    <td>26.27: when finish sending<br>26.1&nbsp;&nbsp;: when remove node frame are not for this node</td>
  </tr>
  <tr>
    <td>S27</td>
    <td>Wait for remove ACK<br>- wait for master ACK<br>- if receive master ACK set it own to not join</td>
    <td>27.1&nbsp;&nbsp;: when both success or time out (not success)</td>
  </tr>
</tbody>
</table>

### How to use
Sample arduino sketch
```cpp
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

// only allow SF7 - SF10 (SF11, SF12 will exceeded max dwell time)
const int           SF = 8;

// power in dBm, no more than 17dBm (50mW)
const int           power_dBm = 11;

// node EUI to accept (you may write it as 0x...)
// MUST BE UNIQUE ACROSS ALL NODE IN ALL NETWORKS for the system to work
const uint64_t      test_EUI_1 = 0xFEDCBA1234567890;  // 16 digits HEX
const uint64_t      test_EUI_2 = 0xABCDEF0987654321;  // 16 digits HEX

// P2PpollingClass (int lora_rx_pin, int lora_tx_pin, int lora_reset_pin);
P2PpollingClass P2Ppolling(RAK4200_RX, RAK4200_TX, RAK4200_RES);

void setup() 
{
  // select master or node, with acknowledgement or without
  // and set SF, power_dBm
  P2Ppolling.P2Ppolling_begin(select_master, ACK_mode, SF, power_dBm);

  // add node EUI to accept (for master)
  P2Ppolling.addtoAccept_EUI(test_EUI_1);
  P2Ppolling.addtoAccept_EUI(test_EUI_2);
  
  // add this node EUI (for node)
  P2Ppolling.setNode_EUI(this_node_EUI);

  // set function to call (for master)
  P2Ppolling.setPrintdatafunc(printData);
  
  // set function to call (for node)
  P2Ppolling.setReadbattfunc(readBatt);
  P2Ppolling.setReadmoisfunc(readMoisture);
  P2Ppolling.setReadtempfunc(readTemp);
  
  // set interval (ms), if not set default to 10s (for master)
  P2Ppolling.setPolling_interval(8000);
  
  // set external led (optional) (for node)
  P2Ppolling.setExled(Exled);
}

void loop() 
{
  // finite state machine of selected type (master or node)
  P2Ppolling.FSM_polling();
}

/*
  to read node data you can use 
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
```
**Explanation**
> P2PpollingClass P2Ppolling(...)
- first create class instance of P2PpollingClass. name it whatever you like, here i name it P2Ppolling and set your pins parameter. you can't have more than 1 instance. it not support

> P2Ppolling.P2Ppolling_begin(...)
- in setup first set it to be master **OR** node and also set mode with **OR** without ACK.

> P2Ppolling.addtoAccepte_EUI(...);
>
> P2Ppolling.setPrintdatafunc(...);
- if you set it to master you have to set node EUI to accept. and set your function that get call every time master finish polling a node.

> P2Ppolling.setNode_EUI(this_node_EUI);
> 
> P2Ppolling.setReadXXXfunc(...);
- if you set it to node you have to set it EUI. and set your function that get call when waiting for master polling.

> P2Ppolling.setPolling_interval(...);
- for master you have to set polling interval. see [Polling interval](#Calculate_polling_interval) how to calculate that so you not exceed 1% duty cycle.

> P2Ppolling.setExled(...);
- for node you can set Exled that will just toggle when it about to send data. i use it for debug. don't set it if you not using it.

> P2Ppolling.FSM_polling();
- both master and node you just use this method to do all the work. it should get call very often so keep the you loop short.

> void printData (uint8_t _addr, int success)
- for master this is the function that you set to get it call every time master finish polling a node.
you can name it what you like **BUT** you must keep function return type (void) and type of it parameter.

> void readXXX (uintXX_t& XXX)
- for node this is the function that you set to get it call when waiting for master polling.
you can name it what you like **BUT** you must keep function return type (void) and type of it parameter.

#### Calculate polling interval

You have to make duty cycle of transmit to be within 1% for master because master will have to transmit more often than node. can be calculate by the number of node you need and number of frame master have to send for all the node and time on air (duration to transmit frame). that mean your interval have to be **at least (99 * total time on air for all transmit frame)** also each spreading factor (SF) will reduce or increase your time on air and polling interval. time no air are in table for difference SF. the polling frame and ACK from master are 4 bytes. and fix bandwidth at 125kHz coding rate of 4/8 and 8 symbol. i'm **not include joining or netAddr negotiate** partly because it will be small part in normal operation (or only at the start if you system not change). but you should keep that in mind as well.

(value from semtech lora modem calculator tool)

<table>
<thead>
  <tr>
    <th>SF/Number of bytes</th>
    <th>4 bytes</th>
    <th>8 bytes</th>
    <th>12 bytes</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td>SF 7</td>
    <td>29ms</td>
    <td>46ms</td>
    <td>54ms</td>
  </tr>
  <tr>
    <td>SF 8</td>
    <td>58ms</td>
    <td>75ms</td>
    <td>91ms</td>
  </tr>
  <tr>
    <td>SF 9</td>
    <td>116ms</td>
    <td>149ms</td>
    <td>182ms</td>
  </tr>
  <tr>
    <td>SF 10</td>
    <td>232ms</td>
    <td>297ms</td>
    <td>297ms</td>
  </tr>
</tbody>
</table>

**Example** : 
1. if you have <ins>10 node and with ACK mode at SF 8</ins> and **if no frame loss** the total time on air are (10 node *2 frame per node * 58ms) = 1160ms the polling interval are at least 99 * 1160 = 11484ms **(114.84 second)**

2. if you have <ins>25 node and without ACK mode at SF 8</ins> the total time on air are (20 node *1 frame per node * 58ms) = 1450ms the polling interval are at least 99 * 1450 = 143550ms **(143.55 second)**

3. if you have <ins>20 node and with ACK mode at SF 8</ins> and **if you need to send 3 frame** the total time on air are (20 node *3 frame per node * 58ms) = 3480ms the polling interval are at least 99 * 3480 = 344520ms **(344.52 second)**

4. if you have <ins>20 node and with ACK mode at SF 10</ins> and **if no frame loss** the total time on air are (20 node *2 frame per node * 232ms) = 9280ms the polling interval are at least 99 * 9280 = 918720ms **(918.72 second) 15 minute and 18.72 second**

![HW_image](/README_assets/HW_pic.jpg)

and this is the picture of ESP32 and RAK4200 that i use

### Note
###### As of version 1.0.0

**There aren't any Encryption or Authentication in here at all**

partly because i'm don't have enough knowledge to **do it correctly**. and i don't want to give false sense of security. both master and node are just read the frame header than do what it should do with it. so keep in mind that there aren't any encryption or authentication. i might try to implement something in the future but i can't make any promise. it up to you if you can take the risk of someone try to send wrong value or they read your sensor. for what i want to use this for it won't bring down larger infrastructure or other part of the system immediately. but you have to keep this in mind.

**Number of network limitation** (not done!)

if you just look at the number of netAddr that can be assign it 223. but i recommend to use no more than 9 for a single network per channel. or if you want to try 2 network per channel for a total of 18 network. if there are more network there will be problem at the netAddr negotiate process because right now if any device send netAddr asking master will answer it right away which will cause frame collision. i want to fix this in the future by add some kind of unique time slot for each network to answer to avoid frame collision as much as possible. and also make node answer netAddr negotiate as well. because in the case of if only node are in the range of the master that asking but not in the range of existing master. right now if that the case the new master might claim the same netAddr as the node and lead to more problem. i want to fix this but i can't really promise. however as you are the one who set the network yourself you should to make less but big network instead or limit it to no more than 18 network for now.

**Compatible hardware limitation**

compatible hardware as of v1.0.0 are ESP32 and RAK4200. for the MCU side my only code that are specific to ESP32 are probably Serial.begin that can set UART pin. so it should not be too hard to support other architecture if it only need UART. but the more problem one are lora module. the RAK4200 (RAK4270 probably work but i don't have any to test) are (sx1276 + stm32) and are easy to setup we just need to know at command and for that i use mod_ETT_RAK4200 which i modify from the file that came from [ETT RAK4200][ETT RAK4200 link] to easy set mode, receive and send frame. this is difference than most "lora" board that are connect with semtech sx127X, sx126X directly with SPI. i want to support this in the future too. but i can't promise and if you want to use this library with something like HopeRF RFM95 than unfortunately you have to modify it your self or if you want to you can contribute to this library to make it work. and you can see my todo for what you should modify or add to support more module.

**Data format limitation**

right now data format that node send to master are fix. this is again because my own lack of necessary knowledge to make it to be able to change at start up. right now it fix to what i want to use it with. if you want to change that unfortunately you have to modify the code yourself and you can see my todo for what you should modify or add to support that.

###### Todo and note to self from author

I am probably the only one who read all this. so i will use space here for note to myself about why i make this library. first just from the limitation this library is not so complete and not in the same standard as many arduino library. but the reason i still put it here. because i might not be able to update this or maybe only do some minor fix because what i want to do with it don't need that much functionality. i can just keep it to myself but i spend a lot of time and effort making it. so i think even though this library aren't that good or seem incomplete i want to make that effort useful maybe someone see this library ideal and decide to make a better one. and also i use and like many open source software (or FOSS in general) and someday i want to contribute as well so i think this library even though it probably not that useful but it the first step for me toward making a better one or first step to support more source software for me.

- what to modify or add to support more module. i probably add another layer between P2Ppolling class (FSM) and module driver and have FSM call this middle layer instead. this middle will replace what is  right now came from mod_ETT_RAK4200. which are
	- initial lora module Working Mode to P2P (RakLoRa.rk_setWorkingMode)
	- set P2P parameter (RakLoRa.rk_initP2P)
	- set P2P to receiver or transmitter (RakLoRa.rk_initP2P_mode)
	- send frame (RakLoRa.rk_sendP2PData)
	- receive frame (it just read back at_command from RAK4200) (RakLoRa.rk_recvP2PData)
	
- look into use another frequency instead of mimic LoRaWAN AS923. i think about using 920MHz - 923MHz. with that i can have 15 channel. and also i think about reduce channel bandwidth to only 150kHz to increase nuber of channel further to 20 as well.

![spectrum_image](/README_assets/RTL-SDR_crop.png)

this is the spectrum of all 9 channel using right now. from RTL-SDR using SDR#

- the data that node send to master are fix in the number of fields and length. i should add a method to set the data format at compile time. and that set the same data format for all the master and node but right now i still learning how to do something like that so again i can't promise that i will add that functionality.  

- this library use [CircularBuffer][CircularBuffer link] to create FIFO buffer which is kind of a queue for master to process join request in order from first to last. i use it because when i try to implement on my own it take too much time and still not really have all the functionality that i want. so i think don't need to reinvent the wheel and take what available now. but in the future i should just implement my own FIFO buffer and make this library less dependent on external library.

- right now i only have 3 RAK4200 for testing so there might be some thing i overlook.

[กฏ กสทช. link]: https://www.nbtc.go.th/spectrum_management/%E0%B8%81%E0%B8%B2%E0%B8%A3%E0%B8%88%E0%B8%B1%E0%B8%94%E0%B8%AA%E0%B8%A3%E0%B8%A3%E0%B8%84%E0%B8%A5%E0%B8%B7%E0%B9%88%E0%B8%99%E0%B8%84%E0%B8%A7%E0%B8%B2%E0%B8%A1%E0%B8%96%E0%B8%B5%E0%B9%88-(1)/%E0%B8%81%E0%B8%B2%E0%B8%A3%E0%B8%88%E0%B8%B1%E0%B8%94%E0%B8%AA%E0%B8%A3%E0%B8%A3%E0%B9%81%E0%B8%A5%E0%B8%B0%E0%B8%AD%E0%B8%99%E0%B8%B8%E0%B8%8D%E0%B8%B2%E0%B8%95%E0%B9%83%E0%B8%AB%E0%B9%89%E0%B9%83%E0%B8%8A%E0%B9%89%E0%B8%84%E0%B8%A5%E0%B8%B7%E0%B9%88%E0%B8%99%E0%B8%84%E0%B8%A7%E0%B8%B2%E0%B8%A1%E0%B8%96%E0%B8%B5%E0%B9%88/28672.aspx?lang=th-th

[lora vs LoRaWAN link]: https://lora-developers.semtech.com/documentation/tech-papers-and-guides/lora-and-lorawan/

[catLoRa link]: https://loraiot.cattelecom.com/site/home

[The Things Indoor Gateway link]: https://www.thethingsnetwork.org/docs/gateways/thethingsindoor/

[ttn fair-use link]: https://www.thethingsnetwork.org/forum/t/fair-use-policy-explained/1300

[pygate link]: https://pycom.io/product/pygate/

[WiPy link]: https://pycom.io/product/wipy-3-0/

[CircularBuffer link]: https://github.com/rlogiacco/CircularBuffer

[time on air calculator link]: https://avbentem.github.io/airtime-calculator/ttn/as923

[ETT RAK4200 link]: http://www.etteam.com/prodIOT/LORA-RAK4200-AS923-MODULE/index.html
