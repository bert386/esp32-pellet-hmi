//
#include <EEPROM.h>
#include <Wire.h>
#include "pcf8574_esp.h"

//
#include "OneWire.h"
#include "DallasTemperature.h"

//
#include "DHT.h"

//
#include "FiniteStateMachine.h"

//
#include "Nextion.h"

//
#include "FBD.h"

#include "dht_nonblocking.h"

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

//
static uint32_t nOnewireMillis = 0;
static uint32_t nDHTMillis = 0;
static uint32_t nRelayMillis = 0;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	if (type == WS_EVT_CONNECT)
	{
		Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
		client->printf("Hello Client %u :)", client->id());
		client->ping();
	}
	else if (type == WS_EVT_DISCONNECT)
	{
		Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
	}
	else if (type == WS_EVT_ERROR)
	{
		Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
	}
	else if (type == WS_EVT_PONG)
	{
		Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
	}
	else if (type == WS_EVT_DATA)
	{
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		String msg = "";
		if (info->final && info->index == 0 && info->len == len)
		{
			//the whole message is in a single frame and we got all of it's data
			Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

			if (info->opcode == WS_TEXT)
			{
				for (size_t i = 0; i < info->len; i++)
				{
					msg += (char)data[i];
				}
			}
			else
			{
				char buff[3];
				for (size_t i = 0; i < info->len; i++)
				{
					sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			Serial.printf("%s\n", msg.c_str());

			if (info->opcode == WS_TEXT)
				client->text("I got your text message");
			else
				client->binary("I got your binary message");
		}
		else
		{
			//message is comprised of multiple frames or the frame is split into multiple packets
			if (info->index == 0)
			{
				if (info->num == 0)
					Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
				Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
			}

			Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

			if (info->opcode == WS_TEXT)
			{
				for (size_t i = 0; i < info->len; i++)
				{
					msg += (char)data[i];
				}
			}
			else
			{
				char buff[3];
				for (size_t i = 0; i < info->len; i++)
				{
					sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			Serial.printf("%s\n", msg.c_str());

			if ((info->index + len) == info->len)
			{
				Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
				if (info->final)
				{
					Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
					if (info->message_opcode == WS_TEXT)
						client->text("I got your text message");
					else
						client->binary("I got your binary message");
				}
			}
		}
	}
}

const char *ssid = "Lenex Home of Horrible Things";
const char *password = "JoeJoeJe11yBean5";
const char *hostName = "controller_Ver1.4";
const char *http_username = "admin";
const char *http_password = "admin";

// error level definitions
#ifdef DEVELOPMENT
#define DOORSWITCHERRORLEVEL 0
#define VACSWITCHERRORLEVEL 0
#define OVERTEMPEXHERRORLEVEL 0
#define OVERTEMPWATERRORLEVEL 0
#define EMPTYPELLETERRORLEVEL 0
#define WATERLOWERRORLEVEL 0
#define PHOTOERRORLEVEL 0
#define PIRMOTIONENABLE 0
#else
#define DOORSWITCHERRORLEVEL 1
#define VACSWITCHERRORLEVEL 1
#define OVERTEMPEXHERRORLEVEL 1
#define OVERTEMPWATERRORLEVEL 1
#define EMPTYPELLETERRORLEVEL 1
#define WATERLOWERRORLEVEL 1
#define PHOTOERRORLEVEL 1
#define PIRMOTIONENABLE 1
#endif

// repeat count setting in start cycle
uint8_t STARTCYCLEREPEATCOUNT = 3;

const char CONFIG_VERSION[4] = "CN4";
#define CONFIG_START 0x10

const uint8_t SDAPIN = 4;
const uint8_t SCLPIN = 5;

// Addresses for PCF8574
#ifdef DEVELOPMENT
const uint8_t PCF8574ADDR1 = 0x20; // output module
const uint8_t PCF8574ADDR2 = 0x21; // input module
#else
const uint8_t PCF8574ADDR1 = 0x38; // output module
const uint8_t PCF8574ADDR2 = 0x3C; // input module
#endif

PCF857x gpioExpanderOut(PCF8574ADDR1, &Wire);
PCF857x gpioExpanderIn(PCF8574ADDR2, &Wire);

// onewire for DS18B20
const uint8_t ONWIREBUS = 12;
OneWire oneWire(ONWIREBUS);
DallasTemperature dsSensors(&oneWire);

const uint8_t FIRSTINDEX = 0;
const uint8_t SECONDINDEX = 1;

const uint8_t DHT1PIN = 13;
const uint8_t DHT2PIN = 0;

#define DHT_SENSOR_TYPE DHT_TYPE_22

static const int DHT_SENSOR_PIN = 0;
DHT_nonblocking dht1(DHT1PIN, DHT_SENSOR_TYPE);
DHT_nonblocking dht2(DHT2PIN, DHT_SENSOR_TYPE);

/*
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht1(DHT1PIN, DHTTYPE);
DHT dht2(DHT2PIN, DHTTYPE);
*/

void standbyEnter();
void standbyUpdate();

void startCycleEnter();
void startCycleUpdate();

void heatCycleEnter();
void heatCycleUpdate();

void stopCycleEnter();
void stopCycleUpdate();

State standby = State(standbyEnter, standbyUpdate, NULL);
State startCycle = State(startCycleEnter, startCycleUpdate, NULL);
State heatCycle = State(heatCycleEnter, heatCycleUpdate, NULL);
State stopCycle = State(stopCycleEnter, stopCycleUpdate, NULL);
FSM controlFSM = FSM(standby);

void mainPageEnter();
void mainPageUpdate();
void diag01PageEnter();
void diag01PageUpdate();

void diag02ScreenEnter();
void diag02ScreenUpdate();

void diag03ScreenEnter();
void diag03ScreenUpdate();

void diag04ScreenEnter();
void diag04ScreenUpdate();

void setting01ScreenEnter();
void setting01ScreenUpdate();

void setting02ScreenEnter();
void setting02ScreenUpdate();

void setting03ScreenEnter();
void setting03ScreenUpdate();

void tempscreenEnter();
void tempscreenUpdate();

void erorrviewEnter();
void erorrviewUpdate();

void onHeap(AsyncWebServerRequest *request);
void onGetSetting(AsyncWebServerRequest *request);

// screen status
State mainScreen = State(mainPageEnter, mainPageUpdate, NULL);
State diag01Screen = State(diag01PageEnter, diag01PageUpdate, NULL);
State diag02Screen = State(diag02ScreenEnter, diag02ScreenUpdate, NULL);
State diag03Screen = State(diag03ScreenEnter, diag03ScreenUpdate, NULL);
State diag04Screen = State(diag04ScreenEnter, diag04ScreenUpdate, NULL);
State setting01Screen = State(setting01ScreenEnter, setting01ScreenUpdate, NULL);
State setting02Screen = State(setting02ScreenEnter, setting02ScreenUpdate, NULL);
State setting03Screen = State(setting03ScreenEnter, setting03ScreenUpdate, NULL);
State temperScreen = State(tempscreenEnter, tempscreenUpdate, NULL);
State erorrview = State(erorrviewEnter, erorrviewUpdate, NULL);

FSM screenFSM = FSM(mainScreen);

//
NexPage mainPage = NexPage(0, 0, "mainview");
NexText waterTemptxt = NexText(0, 13, "waterTemp");

NexButton settingbutton = NexButton(0, 12, "settingbutton");
NexButton diagbutton = NexButton(0, 17, "diagbutton");
NexButton tempbutton = NexButton(0, 18, "tempbutton");
NexButton alarmbutton = NexButton(0, 19, "alarmbutton");
NexDSButton startbutton = NexDSButton(0, 20, "startbutton");

NexVariable systemFault = NexVariable(0, 21, "fault");

// diagnostic screen 01
NexPage diagnos01Page = NexPage(1, 0, "diagnostic01");
NexButton diag01ButtonDown = NexButton(1, 3, "buttonDown");
NexButton diag01ButtonHome = NexButton(1, 1, "home");
NexText diag01PelletTemp = NexText(1, 6, "pelletTemp");
NexText diag01WaterTemp = NexText(1, 9, "waterTemp");
NexText diag01Convection = NexText(1, 14, "convectionTemp");
NexText diag01ExhaustTemp = NexText(1, 17, "exhaustTemp");

// diagnostic screen 02
NexPage diagnos02Page = NexPage(2, 0, "diagnostic02");
NexButton diag02ButtonUp = NexButton(2, 2, "buttonUp");
NexButton diag02ButtonDown = NexButton(2, 3, "buttonDown");
NexButton diag02ButtonHome = NexButton(2, 1, "home");
NexText diag02humidity1 = NexText(2, 5, "humidity1");
NexText diag02humidity2 = NexText(2, 8, "humidity2");

// diagnostic screen 03
NexPage diagnos03Page = NexPage(3, 0, "diagnostic03");
NexButton diag03ButtonUp = NexButton(3, 1, "buttonUp");
NexButton diag03ButtonDown = NexButton(3, 2, "buttonDown");
NexButton diag03ButtonHome = NexButton(3, 17, "home");

NexDSButton diag03blowerRelay = NexDSButton(3, 4, "blowerRelay");
NexDSButton diag03vacFillerRelay = NexDSButton(3, 10, "vacFillerRelay");
NexDSButton diag03exhaustRelay = NexDSButton(3, 6, "exhaustRelay");
NexDSButton diag03pumpRelay = NexDSButton(3, 12, "pumpRelay");
NexDSButton diag03feederRelay = NexDSButton(3, 8, "feederRelay");
NexDSButton diag03heaterRelay = NexDSButton(3, 14, "heaterRelay");
NexDSButton diag03buildupRelay = NexDSButton(3, 16, "buildupRelay");

// diagnostic screen 04
NexPage diagnos04Page = NexPage(4, 0, "diagnostic04");
NexButton diag04ButtonUp = NexButton(4, 5, "buttonUp");
NexButton diag04ButtonDown = NexButton(4, 6, "buttonDown");
NexButton diag04ButtonHome = NexButton(4, 4, "home");

NexPicture diag04DoorSwitch = NexPicture(4, 1, "doorSwitch");
NexPicture diag04WaterOverTemp = NexPicture(4, 11, "waterOverTemp");
NexPicture diag04VacSwitch = NexPicture(4, 2, "vacSwitch");
NexPicture diag04PelletEmpty = NexPicture(4, 14, "pelletEmpty");
NexPicture diag04ExhausticOVT = NexPicture(4, 3, "exhausticOVT");
NexPicture diag04WaterLow = NexPicture(4, 17, "waterLow");
NexPicture diag04PhotoSensor = NexPicture(4, 13, "photoSensor");

// setting screen 01
NexPage setting01Page = NexPage(5, 0, "setting01");
NexButton setting01ButtonUp = NexButton(5, 5, "buttonUp");
NexButton setting01ButtonDown = NexButton(5, 6, "buttonDown");
NexButton setting01ButtonHome = NexButton(5, 4, "home");
NexButton setting01ButtonSave = NexButton(5, 7, "buttonSave");

NexText settingTemp = NexText(5, 9, "settingTemp");
NexNumber setting01Timer1 = NexNumber(5, 15, "timer1");
NexNumber setting01Timer2 = NexNumber(5, 16, "timer2");
NexNumber setting01Timer3 = NexNumber(5, 17, "timer3");

// setting screen 02
NexPage setting02Page = NexPage(6, 0, "setting02");
NexButton setting02ButtonUp = NexButton(6, 2, "buttonUp");
NexButton setting02ButtonDown = NexButton(6, 3, "buttonDown");
NexButton setting02ButtonHome = NexButton(6, 1, "home");
NexButton setting02ButtonSave = NexButton(6, 4, "buttonSave");

NexNumber setting02Timer4 = NexNumber(6, 13, "timer4");
NexNumber setting02Timer5 = NexNumber(6, 14, "timer5");
NexNumber setting02Timer6 = NexNumber(6, 15, "timer6");
NexNumber setting02Timer7 = NexNumber(6, 16, "timer7");
NexNumber setting02Timer8 = NexNumber(6, 18, "timer8");
NexNumber setting02Timer9 = NexNumber(6, 21, "timer9");

// setting screen 03
NexPage setting03Page = NexPage(7, 0, "setting03");
NexButton setting03ButtonUp = NexButton(7, 2, "buttonUp");
NexButton setting03ButtonDown = NexButton(7, 3, "buttonDown");
NexButton setting03ButtonHome = NexButton(7, 1, "home");
NexButton setting03ButtonSave = NexButton(7, 4, "buttonSave");

NexNumber setting03SavingTime = NexNumber(7, 40, "screenSaving");

// temperature screen
NexPage temperPage = NexPage(9, 0, "tempscreen");
NexButton temperButtonHome = NexButton(9, 9, "home");
NexText waterTemp = NexText(9, 2, "waterTemp");
NexText exhaustTemp = NexText(9, 5, "exhaustTemp");

// error view page
NexPage errorPage = NexPage(10, 0, "erorrview");
NexButton errorButtonHome = NexButton(10, 2, "home");
NexText errorDesctxt = NexText(10, 1, "errorDesc");

NexTouch *nex_listen[] =
	{
		&settingbutton,
		&diagbutton,
		&tempbutton,
		&alarmbutton,
		&startbutton,

		&diag01ButtonDown,
		&diag01ButtonHome,

		&diag02ButtonUp,
		&diag02ButtonDown,
		&diag02ButtonHome,

		&diag03ButtonUp,
		&diag03ButtonDown,
		&diag03ButtonHome,

		&diag03blowerRelay,
		&diag03vacFillerRelay,
		&diag03exhaustRelay,
		&diag03pumpRelay,
		&diag03feederRelay,
		&diag03heaterRelay,
		&diag03buildupRelay,

		&diag04ButtonUp,
		&diag04ButtonDown,
		&diag04ButtonHome,

		&setting01ButtonUp,
		&setting01ButtonDown,
		&setting01ButtonHome,
		&setting01ButtonSave,

		&setting02ButtonUp,
		&setting02ButtonDown,
		&setting02ButtonHome,
		&setting02ButtonSave,

		&setting03ButtonUp,
		&setting03ButtonDown,
		&setting03ButtonHome,
		&setting03ButtonSave,

		&temperButtonHome,

		&errorButtonHome,
		NULL};

// device variable
union pcf8574Out
{
	struct
	{
		unsigned blowerFan : 1;
		unsigned exhaustFan : 1;
		unsigned feeder : 1;
		unsigned vacFiller : 1;
		unsigned pump : 1;
		unsigned heater : 1;
		unsigned buildupShaker : 1;
		unsigned : 1;
	} Relays;
	uint8_t data;
};

union pcf8574In
{
	struct
	{
		unsigned doorSwitch : 1;
		unsigned vacSwitch : 1;
		unsigned overTempExh : 1;
		unsigned overTempWater : 1;
		unsigned emptyPellets : 1;
		unsigned waterLow : 1;
		unsigned photoSensor : 1;
		unsigned pirMotion : 1;
	} Switches;
	uint8_t data;
};

pcf8574Out prevOut;

union systemfault
{
	struct
	{
		unsigned ds1error : 1;
		unsigned ds2error : 1;
		unsigned dht1error : 1;
		unsigned dht2error : 1;
		unsigned relaysErr : 1;
		unsigned switchesErr : 1;
		unsigned temp : 2;

		unsigned doorErrStart : 1;
		unsigned vacErrStart : 1;
		unsigned ovtExhStart : 1;
		unsigned ovtwaterStart : 1;
		unsigned photoErrStart : 1;
		unsigned overTempHeat : 1;
		unsigned doorErrHeat : 1;
		unsigned waterLowErr : 1;

		unsigned emptyPelletsErr : 1;
		unsigned temp1 : 15;
	} errors;
	uint32_t data;
};

struct
{
	char version[4];

	systemfault fault;

	double pelletTemp;
	double humidity01;

	double waterTemp;
	double humidity02;

	double convectionTemp;
	double exhaustTemp;

	pcf8574Out output;
	pcf8574In input;

	double tempSetting; //

	uint32_t timer1Interval;
	uint32_t timer2Interval;
	uint32_t timer3Interval;
	uint32_t timer4Interval;
	uint32_t timer5Interval;
	uint32_t timer6Interval;
	uint32_t timer7Interval;
	uint32_t timer8Interval;
	uint32_t timer9Interval;

	uint16_t screenSavingTime;
} controller;

// start cycle
TP startFeederTP, startHeaterTP;
Ftrg startFeederTrg, startHeaterTrg;
uint8_t nStartCycleRepeatCnt = 0;
TP startVacFillerTP;

// stop cycle
TON stopFan1TON, stopPumpTON;
TP stopVacFillerTP;

// heat cycle
TP heatFeederOnTON, heatFeederOffTON, heatShakerOnTON, heatShakerOffTON;
TON check90secTON, checkTemp2TON;
Rtrg check90secTrg;

TOF sleepTOF;
Ftrg sleepTrg;
Rtrg wakeTrg, touchTrg;

TP heatVacFillerTP1, heatVacFillerTP2;
Ftrg heatVacFillerTrg1, heatVacFillerTrg2;

TON heatPhotoErrTON;

void initFBDs()
{
	//
	sleepTOF.IN = false;
	sleepTOF.PRE = false;
	sleepTOF.Q = false;
	sleepTOF.PT = controller.screenSavingTime;
	sleepTOF.ET = millis();

	sleepTrg.IN = false;
	sleepTrg.PRE = false;
	sleepTrg.Q = false;

	wakeTrg.IN = false;
	wakeTrg.PRE = false;
	wakeTrg.Q = false;

	touchTrg.IN = false;
	touchTrg.PRE = false;
	touchTrg.Q = false;

	//
	startFeederTP.IN = false;
	startFeederTP.PRE = false;
	startFeederTP.Q = false;
	startFeederTP.PT = controller.timer1Interval * 1000;
	startFeederTP.ET = 0;

	startHeaterTP.IN = false;
	startHeaterTP.PRE = false;
	startHeaterTP.Q = false;
	startHeaterTP.PT = controller.timer2Interval * 1000;
	startHeaterTP.ET = 0;

	startFeederTrg.IN = false;
	startFeederTrg.Q = false;
	startFeederTrg.PRE = false;

	startHeaterTrg.IN = false;
	startHeaterTrg.Q = false;
	startHeaterTrg.PRE = false;

	stopFan1TON.IN = false;
	stopFan1TON.PRE = false;
	stopFan1TON.Q = false;
	stopFan1TON.PT = (controller.timer4Interval * 1000);
	stopFan1TON.ET = 0;

	stopPumpTON.IN = false;
	stopPumpTON.PRE = false;
	stopPumpTON.Q = false;
	stopPumpTON.PT = (controller.timer3Interval * 1000);
	stopPumpTON.ET = 0;

	//
	heatFeederOnTON.IN = false;
	heatFeederOnTON.PRE = false;
	heatFeederOnTON.Q = false;
	heatFeederOnTON.PT = controller.timer5Interval * 1000;
	heatFeederOnTON.ET = 0;

	heatFeederOffTON.IN = false;
	heatFeederOffTON.PRE = false;
	heatFeederOffTON.Q = false;
	heatFeederOffTON.PT = controller.timer6Interval * 1000;
	heatFeederOffTON.ET = 0;

	check90secTON.IN = false;
	check90secTON.PRE = false;
	check90secTON.Q = false;
	check90secTON.PT = 90 * 1000;
	check90secTON.ET = 0;

	check90secTrg.IN = false;
	check90secTrg.Q = false;
	check90secTrg.PRE = false;

	heatShakerOnTON.IN = false;
	heatShakerOnTON.PRE = false;
	heatShakerOnTON.Q = false;
	heatShakerOnTON.PT = controller.timer8Interval * 1000;
	heatShakerOnTON.ET = 0;

	heatShakerOffTON.IN = false;
	heatShakerOffTON.PRE = false;
	heatShakerOffTON.Q = false;
	heatShakerOffTON.PT = controller.timer9Interval * 1000;
	heatShakerOffTON.ET = 0;

	checkTemp2TON.IN = false;
	checkTemp2TON.PRE = false;
	checkTemp2TON.Q = false;
	checkTemp2TON.PT = 300 * 1000;
	checkTemp2TON.ET = 0;
}

bool loadConfig()
{
	bool bResult = true;
	for (unsigned int t = 0; t < sizeof(controller); t++)
		*((char *)&controller + t) = EEPROM.read(CONFIG_START + t);

	if (controller.version[0] == CONFIG_VERSION[0] &&
		controller.version[1] == CONFIG_VERSION[1] &&
		controller.version[2] == CONFIG_VERSION[2])
	{
		bResult = true;
	}
	else
	{
		initVars();
		bResult = false;
	}

	return bResult;
}

void saveConfig()
{
	controller.version[0] = CONFIG_VERSION[0];
	controller.version[1] = CONFIG_VERSION[1];
	controller.version[2] = CONFIG_VERSION[2];

	for (unsigned int t = 0; t < sizeof(controller); t++)
		EEPROM.write(CONFIG_START + t, *((char *)&controller + t));
	EEPROM.commit();
}

void initVars()
{

	controller.pelletTemp = 0.0F;
	controller.humidity01 = 0.0F;
	controller.waterTemp = 0.0F;
	controller.humidity02 = 0.0F;
	controller.convectionTemp = 0.0F;
	controller.exhaustTemp = 0.0F;

	controller.tempSetting = 35.6;
	controller.timer1Interval = 90;
	controller.timer2Interval = 90;
	controller.timer3Interval = 90;
	controller.timer4Interval = 90;
	controller.timer5Interval = 90;
	controller.timer6Interval = 90;
	controller.timer7Interval = 90;
	controller.timer8Interval = 90;
	controller.timer9Interval = 90;
	controller.screenSavingTime = 10;
}

void touchInit()
{
	settingbutton.attachPop(settingbuttonCallback);
	diagbutton.attachPop(diagbuttonCallback);
	tempbutton.attachPop(tempbuttonCallback);
	alarmbutton.attachPop(alarmbuttonCallback);

	startbutton.attachPop(startbuttonCallback);

	diag01ButtonHome.attachPop(homeButtonCallback);
	diag01ButtonDown.attachPop(diag01ButtonDownCallback);

	diag02ButtonHome.attachPop(homeButtonCallback);
	diag02ButtonUp.attachPop(diag02ButtonUpCallback);
	diag02ButtonDown.attachPop(diag02ButtonDownCallback);

	diag03ButtonUp.attachPop(diag03ButtonUpCallback);
	diag03ButtonDown.attachPop(diag03ButtonDownCallback);
	;
	diag03ButtonHome.attachPop(homeButtonCallback);

	diag03blowerRelay.attachPop(diag03blowerRelayCallback);
	diag03exhaustRelay.attachPop(diag03exhaustRelayCallback);
	diag03vacFillerRelay.attachPop(diag03vacFillerRelayCallback);
	diag03feederRelay.attachPop(diag03feederRelayCallback);
	diag03pumpRelay.attachPop(diag03pumpRelayCallback);
	diag03heaterRelay.attachPop(diag03heaterRelayCallback);
	diag03buildupRelay.attachPop(diag03buildupRelayCallback);

	diag04ButtonUp.attachPop(diag04ButtonUpCallback);
	diag04ButtonDown.attachPop(diag04ButtonDownCallback);
	;
	diag04ButtonHome.attachPop(homeButtonCallback);

	setting01ButtonUp.attachPop(setting01ButtonUpCallback);
	setting01ButtonDown.attachPop(setting01ButtonDownCallback);
	setting01ButtonHome.attachPop(homeButtonCallback);
	setting01ButtonSave.attachPop(setting01ButtonSaveCallback);

	setting02ButtonUp.attachPop(setting02ButtonUpCallback);
	setting02ButtonDown.attachPop(setting02ButtonDownCallback);
	setting02ButtonHome.attachPop(homeButtonCallback);
	setting02ButtonSave.attachPop(setting02ButtonSaveCallback);

	setting03ButtonUp.attachPop(setting03ButtonUpCallback);
	setting03ButtonDown.attachPop(setting03ButtonDownCallback);
	setting03ButtonHome.attachPop(homeButtonCallback);
	setting03ButtonSave.attachPop(setting03ButtonSaveCallback);

	temperButtonHome.attachPop(homeButtonCallback);
	errorButtonHome.attachPop(homeButtonCallback);
}

void setup()
{
	Serial.begin(9600);
	Serial.setDebugOutput(true);
	//WiFi.mode(WIFI_AP);
	WiFi.mode(WIFI_AP_STA);
	WiFi.hostname(hostName);
	WiFi.softAP(hostName);

	WiFi.begin(ssid, password);
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Serial.printf("STA: Failed!\n");
		WiFi.disconnect(false);
		delay(1000);
		WiFi.begin(ssid, password);
	} //*/

	WiFi.setOutputPower(20.5F);
	MDNS.addService("http", "tcp", 80);
	SPIFFS.begin();

	ws.onEvent(onWsEvent);
	server.addHandler(&ws);

	events.onConnect([](AsyncEventSourceClient *client) {
		client->send("hello!", NULL, millis(), 1000);
	});
	server.addHandler(&events);

	server.addHandler(new SPIFFSEditor(http_username, http_password));

	server.on("/heap", HTTP_GET, onHeap);
	server.on("/getsetting", HTTP_GET, onGetSetting);
	server.on("/getstatus", HTTP_GET, onGetStatus);
	server.on("/setting", HTTP_GET, onSettingTime);
	server.on("/savetemp", HTTP_GET, onSaveTemp);
	server.on("/relay", HTTP_GET, onRelayToggle);
	server.on("/cycle", HTTP_GET, onCycleToggle);

	server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

	server.onNotFound([](AsyncWebServerRequest *request) {
		Serial.printf("NOT_FOUND: ");
		if (request->method() == HTTP_GET)
			Serial.printf("GET");
		else if (request->method() == HTTP_POST)
			Serial.printf("POST");
		else if (request->method() == HTTP_DELETE)
			Serial.printf("DELETE");
		else if (request->method() == HTTP_PUT)
			Serial.printf("PUT");
		else if (request->method() == HTTP_PATCH)
			Serial.printf("PATCH");
		else if (request->method() == HTTP_HEAD)
			Serial.printf("HEAD");
		else if (request->method() == HTTP_OPTIONS)
			Serial.printf("OPTIONS");
		else
			Serial.printf("UNKNOWN");
		Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

		if (request->contentLength())
		{
			Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
			Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
		}

		int headers = request->headers();
		int i;
		for (i = 0; i < headers; i++)
		{
			AsyncWebHeader *h = request->getHeader(i);
			Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
		}

		int params = request->params();
		for (i = 0; i < params; i++)
		{
			AsyncWebParameter *p = request->getParam(i);
			if (p->isFile())
			{
				Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
			}
			else if (p->isPost())
			{
				Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
			}
			else
			{
				Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
			}
		}

		request->send(404);
	});
	server.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
		if (!index)
			Serial.printf("UploadStart: %s\n", filename.c_str());
		Serial.printf("%s", (const char *)data);
		if (final)
			Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
	});
	server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		if (!index)
			Serial.printf("BodyStart: %u\n", total);
		Serial.printf("%s", (const char *)data);
		if (index + len == total)
			Serial.printf("BodyEnd: %u\n", total);
	});
	server.begin();

	EEPROM.begin(512);

	// initialize wire
	Wire.begin(SDAPIN, SCLPIN);

	// init PCF8574T module
	gpioExpanderOut.begin();
	gpioExpanderIn.begin();

	// initialize ds18b20 chips
	dsSensors.begin();

	/*
	//
	dht1.begin();
	dht2.begin();
	*/

	/* Set the baudrate which is for debug and communicate with Nextion screen. */
	nexInit();
	touchInit();

	//
	loadConfig();
	initFBDs();

	//
	controller.fault.data = 0;

	//
	controller.output.data = 0;
	controller.input.data = 0;
}

/*
* Poll for a measurement, keeping the state machine alive.  Returns
* true if a measurement is available.
*/

static bool measure1st(float *temp, float *humi)
{
	static unsigned long measurement_timestamp1 = millis();

	/* Measure once every four seconds. */
	if (millis() - measurement_timestamp1 > 4000ul)
	{
		if (dht1.measure(temp, humi) == true)
		{
			measurement_timestamp1 = millis();
			return (true);
		}
	}

	return (false);
}

static bool measure2nd(float *temp, float *humi)
{
	static unsigned long measurement_timestamp2 = millis();

	/* Measure once every four seconds. */
	if (millis() - measurement_timestamp2 > 4000ul)
	{
		if (dht2.measure(temp, humi) == true)
		{
			measurement_timestamp2 = millis();
			return (true);
		}
	}
	return (false);
}

void loop()
{
	//
	const uint32_t ONEWIREINTERVAL = 1000;
	if (millis() - nOnewireMillis > ONEWIREINTERVAL)
	{
		nOnewireMillis = millis();
		//
		dsSensors.requestTemperatures();
		controller.convectionTemp = dsSensors.getTempFByIndex(FIRSTINDEX);
		if (controller.convectionTemp <= -190.0F)
			controller.fault.errors.ds1error = true;
		else
			controller.fault.errors.ds1error = false;

		controller.exhaustTemp = dsSensors.getTempFByIndex(SECONDINDEX);
		if (controller.exhaustTemp <= -190.0F)
			controller.fault.errors.ds2error = true;
		else
			controller.fault.errors.ds2error = false;
	}

	//
	static uint32_t dht1_timestamp = millis();
	static uint32_t dht2_timestamp = millis();
	float temp1, temp2, humi1, humi2;
	// Measure once every four seconds.
	if (millis() - dht1_timestamp > 4000ul)
	{
		if (dht1.measure(&temp1, &humi1) == true)
		{
			controller.pelletTemp = temp1 * 1.8 + 32;
			controller.humidity01 = humi1;
			controller.fault.errors.dht1error = false;
			dht1_timestamp = millis();

			Serial.print("dht1, ");
			Serial.print(controller.pelletTemp, 2);
			Serial.print(",");
			Serial.print(controller.humidity01, 2);
			Serial.println();
		}
		else if (millis() - dht1_timestamp > 16000ul)
		{
			controller.fault.errors.dht1error = true;
			Serial.println("dht1 error");
		}
	}

	if (millis() - dht2_timestamp > 4000ul)
	{
		if (dht2.measure(&temp2, &humi2) == true)
		{
			controller.waterTemp = temp2 * 1.8 + 32;
			controller.humidity02 = humi2;
			controller.fault.errors.dht2error = false;
			dht2_timestamp = millis();

			Serial.print("dht2, ");
			Serial.print(controller.waterTemp, 2);
			Serial.print(",");
			Serial.print(controller.humidity02, 2);
			Serial.println();
		}
		else if (millis() - dht2_timestamp > 16000ul)
		{
			Serial.println("dht2 error");
			controller.fault.errors.dht2error = true;
		}
	}

	//
	const uint32_t RELAYUPDATECYCLE = 250;
	if (millis() - nRelayMillis > RELAYUPDATECYCLE)
	{
		nRelayMillis = millis();

		uint8_t error = 0;
		Wire.beginTransmission(PCF8574ADDR1);
		error = Wire.endTransmission();
		if (error != 0)
		{
			controller.fault.errors.relaysErr = true;
			//Serial.println(F("Output PCF8574 Module does not found."));
		}
		else
		{

			//
			if (prevOut.data != controller.output.data)
			{
				if (screenFSM.isInState(diag03Screen))
				{

					if (controller.output.Relays.blowerFan != prevOut.Relays.blowerFan)
						diag03blowerRelay.setValue(controller.output.Relays.blowerFan);

					if (controller.output.Relays.exhaustFan != prevOut.Relays.exhaustFan)
						diag03exhaustRelay.setValue(controller.output.Relays.exhaustFan);

					if (controller.output.Relays.vacFiller != prevOut.Relays.vacFiller)
						diag03vacFillerRelay.setValue(controller.output.Relays.vacFiller);

					if (controller.output.Relays.feeder != prevOut.Relays.feeder)
						diag03feederRelay.setValue(controller.output.Relays.feeder);

					if (controller.output.Relays.pump != prevOut.Relays.pump)
						diag03pumpRelay.setValue(controller.output.Relays.pump);

					if (controller.output.Relays.heater != prevOut.Relays.heater)
						diag03heaterRelay.setValue(controller.output.Relays.heater);

					if (controller.output.Relays.buildupShaker != prevOut.Relays.buildupShaker)
						diag03buildupRelay.setValue(controller.output.Relays.buildupShaker);
				}

				prevOut.data = controller.output.data;
			}

			controller.fault.errors.relaysErr = false;
			gpioExpanderOut.write8(~controller.output.data);
		}

		Wire.beginTransmission(PCF8574ADDR2);
		error = Wire.endTransmission();
		if (error != 0)
		{
			controller.fault.errors.switchesErr = true;
			//Serial.println(F("Input PCF8574 Module does not found."));
		}
		else
		{
			controller.fault.errors.switchesErr = false;
			pcf8574In temp;
			temp.data = gpioExpanderIn.read8();
			if (temp.data != controller.input.data)
			{
				if (screenFSM.isInState(diag04Screen))
				{
					if (controller.input.Switches.doorSwitch != temp.Switches.doorSwitch)
					{
						if (controller.input.Switches.doorSwitch == VACSWITCHERRORLEVEL)
							diag04DoorSwitch.setPic(6);
						else
							diag04DoorSwitch.setPic(7);
					}

					if (controller.input.Switches.vacSwitch != temp.Switches.vacSwitch)
					{
						if (controller.input.Switches.vacSwitch == VACSWITCHERRORLEVEL)
							diag04VacSwitch.setPic(6);
						else
							diag04VacSwitch.setPic(7);
					}

					if (controller.input.Switches.overTempExh != temp.Switches.overTempExh)
					{
						if (controller.input.Switches.overTempExh == OVERTEMPEXHERRORLEVEL)
							diag04ExhausticOVT.setPic(6);
						else
							diag04ExhausticOVT.setPic(7);
					}

					if (controller.input.Switches.overTempWater != temp.Switches.overTempWater)
					{
						if (controller.input.Switches.overTempWater == OVERTEMPWATERRORLEVEL)
							diag04WaterOverTemp.setPic(6);
						else
							diag04WaterOverTemp.setPic(7);
					}

					if (controller.input.Switches.emptyPellets != temp.Switches.emptyPellets)
					{
						if (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL)
							diag04PelletEmpty.setPic(6);
						else
							diag04PelletEmpty.setPic(7);
					}

					if (controller.input.Switches.waterLow != temp.Switches.waterLow)
					{
						if (controller.input.Switches.waterLow == WATERLOWERRORLEVEL)
							diag04WaterLow.setPic(6);
						else
							diag04WaterLow.setPic(7);
					}

					if (controller.input.Switches.photoSensor != temp.Switches.photoSensor)
					{
						if (controller.input.Switches.photoSensor == PHOTOERRORLEVEL)
							diag04PhotoSensor.setPic(6);
						else
							diag04PhotoSensor.setPic(7);
					}
				}
				controller.input.data = temp.data;
			}
			controller.input.data = gpioExpanderIn.read8();
		}
	}

	//
	nexLoop(nex_listen);

	// Control logic FSM instance
	controlFSM.update();

	// screen logic FSM instance
	screenFSM.update();

	RTrgFunc(&touchTrg);
	touchTrg.IN = false;

	sleepTOF.IN = touchTrg.Q || (controller.input.Switches.pirMotion == PIRMOTIONENABLE);
	sleepTOF.PT = controller.screenSavingTime * 60000;
	TOFFunc(&sleepTOF);
	sleepTrg.IN = sleepTOF.Q;
	FTrgFunc(&sleepTrg);
	if (sleepTrg.Q)
	{
		Serial.println("entered into sleep");
		EnterSleep();
	}

	wakeTrg.IN = sleepTOF.Q;
	RTrgFunc(&wakeTrg);
	if (wakeTrg.Q)
	{
		Serial.println("wakeup");
		ExitSleep();
	}
}

void mainPageRefresh()
{
	systemFault.setValue(controller.fault.data);
	if (controller.fault.errors.dht2error)
		waterTemptxt.setText("Error");
	else
		waterTemptxt.setText(String(controller.waterTemp, 1).c_str());

	if (controlFSM.isInState(standby))
		startbutton.setValue(0);
	else
		startbutton.setValue(1);
}

void mainPageEnter()
{
	Serial.println(F("home screen"));
	mainPage.show();
	mainPageRefresh();
}

void mainPageUpdate()
{
	if (screenFSM.timeInCurrentState() >= 2000)
	{
		mainPageRefresh();
		screenFSM.resetTime();
	}
}

void homeButtonCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(mainScreen);
}

void settingbuttonCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(setting01Screen);
}

void diagbuttonCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag01Screen);
}

void tempbuttonCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(temperScreen);
}

void startbuttonCallback(void *ptr)
{
	touchTrg.IN = true;
	if (controlFSM.isInState(standby))
	{
		// reset repeat counter
		nStartCycleRepeatCnt = 0;
		controlFSM.transitionTo(startCycle);
	}
	else if (controlFSM.isInState(startCycle) || controlFSM.isInState(heatCycle))
	{
		controlFSM.transitionTo(stopCycle);
	}
}

/* */
void diag01Refresh()
{
	if (controller.fault.errors.dht1error)
		diag01PelletTemp.setText("??.?");
	else
		diag01PelletTemp.setText(String(controller.pelletTemp, 1).c_str());

	if (controller.fault.errors.dht2error)
		diag01WaterTemp.setText("??.?");
	else
		diag01WaterTemp.setText(String(controller.waterTemp, 1).c_str());

	if (controller.fault.errors.ds1error)
		diag01Convection.setText("??.?");
	else
		diag01Convection.setText(String(controller.convectionTemp, 1).c_str());

	if (controller.fault.errors.ds2error)
		diag01ExhaustTemp.setText("??.?");
	else
		diag01ExhaustTemp.setText(String(controller.exhaustTemp, 1).c_str());
}

void diag01PageEnter()
{
	Serial.println(F("diag01 screen"));
	diagnos01Page.show();
	diag01Refresh();
}

void diag01PageUpdate()
{
	if (screenFSM.timeInCurrentState() >= 1000)
	{
		diag01Refresh();
		screenFSM.resetTime();
	}
}

void diag01ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.transitionTo(diag02Screen);
}

void diag02Refresh()
{
	if (controller.fault.errors.dht1error)
		diag02humidity1.setText("??");
	else
		diag02humidity1.setText(String(controller.humidity01, 0).c_str());

	if (controller.fault.errors.dht2error)
		diag02humidity2.setText("??");
	else
		diag02humidity2.setText(String(controller.humidity02, 0).c_str());
}

void diag02ScreenEnter()
{
	Serial.println(F("diag02 screen"));
	diagnos02Page.show();
	diag02Refresh();
}

void diag02ScreenUpdate()
{
	if (screenFSM.timeInCurrentState() >= 2000)
	{
		diag02Refresh();
		screenFSM.resetTime();
	}
}

void diag02ButtonUpCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag01Screen);
}

void diag02ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag03Screen);
}

void diag03Refresh()
{
	diag03blowerRelay.setValue(controller.output.Relays.blowerFan);
	diag03exhaustRelay.setValue(controller.output.Relays.exhaustFan);
	diag03vacFillerRelay.setValue(controller.output.Relays.vacFiller);
	diag03feederRelay.setValue(controller.output.Relays.feeder);
	diag03pumpRelay.setValue(controller.output.Relays.pump);
	diag03heaterRelay.setValue(controller.output.Relays.heater);
	diag03buildupRelay.setValue(controller.output.Relays.buildupShaker);

	prevOut = controller.output;
}

void diag03blowerRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03blowerRelay.getValue(&value);
	if (bResult)
	{
		prevOut.Relays.blowerFan = value;
		controller.output.Relays.blowerFan = value;
	}
}

void diag03exhaustRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03exhaustRelay.getValue(&value);
	if (bResult)
	{
		controller.output.Relays.exhaustFan = value;
		prevOut.Relays.exhaustFan = value;
	}
}

void diag03vacFillerRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03vacFillerRelay.getValue(&value);
	if (bResult)
	{
		controller.output.Relays.vacFiller = value;
		prevOut.Relays.vacFiller = value;
	}
}

void diag03pumpRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03pumpRelay.getValue(&value);
	if (bResult)
	{
		controller.output.Relays.pump = value;
		prevOut.Relays.pump = value;
	}
}

void diag03feederRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03feederRelay.getValue(&value);
	if (bResult)
	{
		prevOut.Relays.feeder = value;
		controller.output.Relays.feeder = value;
	}
}

void diag03buildupRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03buildupRelay.getValue(&value);
	if (bResult)
	{
		prevOut.Relays.buildupShaker = value;
		controller.output.Relays.buildupShaker = value;
	}
}

void diag03heaterRelayCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t value = 0;
	bool bResult = diag03heaterRelay.getValue(&value);
	if (bResult)
	{
		prevOut.Relays.heater = value;
		controller.output.Relays.heater = value;
	}
}

void diag03ScreenEnter()
{

	Serial.println(F("diag03 screen"));
	diagnos03Page.show();
	diag03Refresh();
}

void diag03ScreenUpdate()
{
	/*
	if (screenFSM.timeInCurrentState() >= 2000)
	{
	diag03Refresh();
	screenFSM.resetTime();
	}
	*/
}

void diag03ButtonUpCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag02Screen);
}

void diag03ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag04Screen);
}

void diag04Refresh()
{
	if (controller.input.Switches.doorSwitch == DOORSWITCHERRORLEVEL)
		diag04DoorSwitch.setPic(6);
	else
		diag04DoorSwitch.setPic(7);

	if (controller.input.Switches.vacSwitch == VACSWITCHERRORLEVEL)
		diag04VacSwitch.setPic(6);
	else
		diag04VacSwitch.setPic(7);

	if (controller.input.Switches.overTempExh == OVERTEMPEXHERRORLEVEL)
		diag04ExhausticOVT.setPic(6);
	else
		diag04ExhausticOVT.setPic(7);

	if (controller.input.Switches.overTempWater == OVERTEMPWATERRORLEVEL)
		diag04WaterOverTemp.setPic(6);
	else
		diag04WaterOverTemp.setPic(7);

	if (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL)
		diag04PelletEmpty.setPic(6);
	else
		diag04PelletEmpty.setPic(7);

	if (controller.input.Switches.waterLow == WATERLOWERRORLEVEL)
		diag04WaterLow.setPic(6);
	else
		diag04WaterLow.setPic(7);

	if (controller.input.Switches.photoSensor == PHOTOERRORLEVEL)
		diag04PhotoSensor.setPic(6);
	else
		diag04PhotoSensor.setPic(7);
}

void diag04ScreenEnter()
{
	Serial.println(F("diag04 screen"));
	diagnos04Page.show();
	diag04Refresh();
}

void diag04ScreenUpdate()
{
	/*
	if (screenFSM.timeInCurrentState() >= 2000)
	{
	diag04Refresh();
	screenFSM.resetTime();
	}*/
}

void diag04ButtonUpCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(diag03Screen);
}

void diag04ButtonDownCallback(void *ptr)
{
}

void setting01ScreenEnter()
{
	Serial.println(F("setting01 screen"));
	setting01Page.show();

	settingTemp.setText(String(controller.tempSetting, 1).c_str());
	setting01Timer1.setValue(controller.timer1Interval);
	setting01Timer2.setValue(controller.timer2Interval);
	setting01Timer3.setValue(controller.timer3Interval);
}

void setting01ScreenUpdate()
{
}

void setting01ButtonUpCallback(void *ptr)
{
}

void setting01ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(setting02Screen);
}

void setting01ButtonSaveCallback(void *ptr)
{
	touchTrg.IN = true;
	//
	char buff[20];

	if (settingTemp.getText(buff, 20) > 0)
	{
		Serial.println("None error with getting text!");
		controller.tempSetting = atof(buff);
	}
	else
	{
		Serial.println("Error!");
		return;
	}

	uint32_t timerValue;

	if (setting01Timer1.getValue(&timerValue))
		controller.timer1Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting01Timer2.getValue(&timerValue))
		controller.timer2Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting01Timer3.getValue(&timerValue))
		controller.timer3Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	// Save settings into EEPROM
	saveConfig();
}

void setting02ScreenEnter()
{
	Serial.println(F("setting02 screen"));
	setting02Page.show();

	setting02Timer4.setValue(controller.timer4Interval);
	setting02Timer5.setValue(controller.timer5Interval);
	setting02Timer6.setValue(controller.timer6Interval);
	setting02Timer7.setValue(controller.timer7Interval);
	setting02Timer8.setValue(controller.timer8Interval);
	setting02Timer9.setValue(controller.timer9Interval);
}

void setting02ScreenUpdate()
{
}

void setting02ButtonUpCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(setting01Screen);
}

void setting02ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(setting03Screen);
}

void setting02ButtonSaveCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t timerValue;

	if (setting02Timer4.getValue(&timerValue))
		controller.timer4Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting02Timer5.getValue(&timerValue))
		controller.timer5Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting02Timer6.getValue(&timerValue))
		controller.timer6Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting02Timer7.getValue(&timerValue))
		controller.timer7Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting02Timer8.getValue(&timerValue))
		controller.timer8Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	if (setting02Timer9.getValue(&timerValue))
		controller.timer9Interval = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	// Save settings into EEPROM
	saveConfig();
}

void setting03ScreenEnter()
{
	Serial.println(F("setting03 screen"));
	setting03Page.show();
}

void setting03ScreenUpdate()
{
}

void setting03ButtonUpCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(setting02Screen);
}

void setting03ButtonDownCallback(void *ptr)
{
	touchTrg.IN = true;
	//
}

void setting03ButtonSaveCallback(void *ptr)
{
	touchTrg.IN = true;
	uint32_t timerValue;

	if (setting03SavingTime.getValue(&timerValue))
		controller.screenSavingTime = timerValue;
	else
	{
		Serial.println("Error!");
		return;
	}

	// Save settings into EEPROM
	saveConfig();
}

void tempscreenRefresh()
{
	if (controller.fault.errors.dht2error)
		waterTemp.setText("??.?");
	else
		waterTemp.setText(String(controller.waterTemp, 1).c_str());

	if (controller.fault.errors.ds2error)
		exhaustTemp.setText("??.?");
	else
		exhaustTemp.setText(String(controller.exhaustTemp, 1).c_str());
}

void tempscreenEnter()
{
	Serial.println(F("temp screen "));
	temperPage.show();
	tempscreenRefresh();
}

void tempscreenUpdate()
{
	if (screenFSM.timeInCurrentState() >= 2000)
	{
		tempscreenRefresh();
		screenFSM.resetTime();
	}
}

void alarmbuttonCallback(void *ptr)
{
	touchTrg.IN = true;
	screenFSM.immediateTransitionTo(erorrview);
}

void errorRefresh()
{
	errorDesctxt.setText(getCurrError().c_str());
}

void erorrviewEnter()
{
	Serial.println(F("alarm screen"));
	errorPage.show();
	errorRefresh();
}

void erorrviewUpdate()
{
	if (screenFSM.timeInCurrentState() >= 2000)
	{
		errorRefresh();
		screenFSM.resetTime();
	}
}

void standbyEnter()
{
	displayError();

	//
	controller.output.data = 0x00;
	Serial.println("entered into standby status");
}

void standbyUpdate()
{
}

bool checkIOError()
{
	if ((controller.fault.data & 0x00FF) > 0)
		return true;
	return false;
}

String getCurrError()
{

	if (controller.fault.errors.dht1error)
		return "DHT1 sensor error";
	if (controller.fault.errors.dht2error)
		return "DHT2 sensor error";
	if (controller.fault.errors.ds1error)
		return "convection tube Error";
	if (controller.fault.errors.ds2error)
		return "exhaust sensor Error";
	if (controller.fault.errors.relaysErr)
		return "check output PCF8574";
	if (controller.fault.errors.switchesErr)
		return "check input PCF8574";

	if (controller.fault.errors.doorErrStart)
		return "door switch error in start";
	if (controller.fault.errors.vacErrStart)
		return "VAC switch error in start";
	if (controller.fault.errors.ovtExhStart)
		return "exhaust overtemp in start";
	if (controller.fault.errors.ovtwaterStart)
		return "water overtemp in start";
	if (controller.fault.errors.photoErrStart)
		return "photosensor error in start";
	if (controller.fault.errors.overTempHeat)
		return "overtemp error in heat";
	if (controller.fault.errors.doorErrHeat)
		return "door switch error in heat";
	if (controller.fault.errors.waterLowErr)
		return "water low error in heat";

	if (controller.fault.errors.emptyPelletsErr)
		return "empty pellets error in heat";

	return "None error";
}

void displayError()
{
	if (controller.fault.data != 0)
		Serial.println(getCurrError());
}

void startCycleEnter()
{

	Serial.println(F("entered into start cycle"));
	startFeederTP.IN = false;
	startFeederTP.PRE = false;
	startFeederTP.Q = false;
	startFeederTP.PT = controller.timer1Interval * 1000;
	startFeederTP.ET = 0;

	startHeaterTP.IN = false;
	startHeaterTP.PRE = false;
	startHeaterTP.Q = false;
	startHeaterTP.PT = controller.timer2Interval * 1000;
	startHeaterTP.ET = 0;

	startFeederTrg.IN = false;
	startFeederTrg.Q = false;
	startFeederTrg.PRE = false;

	startHeaterTrg.IN = false;
	startHeaterTrg.Q = false;
	startHeaterTrg.PRE = false;

	startFeederTP.IN = true;

	// reset fault errors
	controller.fault.data &= 0x00FF;

	// reset outputs
	controller.output.data = 0x00;

	startVacFillerTP.IN = false;
	startVacFillerTP.PRE = false;
	startVacFillerTP.Q = false;
	startVacFillerTP.PT = controller.timer7Interval * 1000;
	startVacFillerTP.ET = 0;
}

void startCycleUpdate()
{
	// check io error
	if (checkIOError())
	{
		Serial.println(F("system has io error !"));
		controlFSM.transitionTo(standby);
		return;
	}

	// R1 ON when temp3 is in 100
	controller.output.Relays.blowerFan = (controller.convectionTemp >= 100.0F);

	// R5 ON If 85 <= temp2
	controller.output.Relays.pump = (controller.waterTemp >= 85.0F);

	// R3 (Feeder ON as timer1interval once)
	startFeederTP.PT = controller.timer1Interval * 1000;
	TPFunc(&startFeederTP);
	controller.output.Relays.feeder = startFeederTP.Q;

	// R2 (Exhauster ON)
	controller.output.Relays.exhaustFan = true;

	// check, S1, S2, S3, S4, if no error, heater on in timer2,
	startFeederTrg.IN = startFeederTP.Q;
	FTrgFunc(&startFeederTrg);
	if (startFeederTrg.Q)
	{
		if (controller.input.Switches.doorSwitch == DOORSWITCHERRORLEVEL)
		{
			controller.fault.errors.doorErrStart = 1;
			controlFSM.transitionTo(stopCycle);
			return;
		}

		if (controller.input.Switches.vacSwitch == VACSWITCHERRORLEVEL)
		{
			controller.fault.errors.vacErrStart = 1;
			controlFSM.transitionTo(stopCycle);
			return;
		}

		if (controller.input.Switches.overTempExh == OVERTEMPEXHERRORLEVEL)
		{
			controller.fault.errors.ovtExhStart = 1;
			controlFSM.transitionTo(stopCycle);
			return;
		}

		if (controller.input.Switches.overTempWater == OVERTEMPWATERRORLEVEL)
		{
			controller.fault.errors.ovtwaterStart = 1;
			controlFSM.transitionTo(stopCycle);
			return;
		}

		startHeaterTP.IN = true;
	}

	// No Error, R6 Heater ON as Timer2
	startHeaterTP.PT = controller.timer2Interval * 1000;
	TPFunc(&startHeaterTP);
	controller.output.Relays.heater = startHeaterTP.Q;

	// When Timer2 is completed, check Photosensor error
	startHeaterTrg.IN = startHeaterTP.Q;
	FTrgFunc(&startHeaterTrg);
	if (startHeaterTrg.Q)
	{
		// check photo sensor < setting point, // Error, goto stop cycle
		if (controller.input.Switches.photoSensor == PHOTOERRORLEVEL)
		{
			controller.fault.errors.photoErrStart = 1;
			nStartCycleRepeatCnt++;
			if (nStartCycleRepeatCnt < STARTCYCLEREPEATCOUNT)
			{ // Repeat startCycle
				Serial.println("Repeat Start cycle");
				controlFSM.immediateTransitionTo(startCycle);
			}
			else
			{
				controlFSM.transitionTo(stopCycle);
			}
			return;
		}
		else
		{ // no error, go to heat cycle
			controlFSM.transitionTo(heatCycle);
		}
	}

	// if s6 off, then stop cycle
	if (controller.input.Switches.waterLow == WATERLOWERRORLEVEL)
	{
		controller.fault.errors.waterLowErr = true;
		controlFSM.transitionTo(stopCycle);
	}

	// If Pellets Empty S5 off, then r4 on for time 7
	startVacFillerTP.IN = controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL;
	startVacFillerTP.PT = controller.timer7Interval * 1000;
	TPFunc(&startVacFillerTP);
	controller.output.Relays.vacFiller = startVacFillerTP.Q;
}

void heatCycleEnter()
{
	// reset outputs
	controller.output.data = 0x00;
	Serial.println(F("entered into heat cycle!"));

	//
	heatFeederOnTON.IN = false;
	heatFeederOnTON.PRE = false;
	heatFeederOnTON.Q = false;
	heatFeederOnTON.PT = controller.timer5Interval * 1000;
	heatFeederOnTON.ET = 0;

	heatFeederOffTON.IN = false;
	heatFeederOffTON.PRE = false;
	heatFeederOffTON.Q = false;
	heatFeederOffTON.PT = controller.timer6Interval * 1000;
	heatFeederOffTON.ET = 0;

	check90secTON.IN = false;
	check90secTON.PRE = false;
	check90secTON.Q = false;
	check90secTON.PT = 90 * 1000;
	check90secTON.ET = 0;

	check90secTrg.IN = false;
	check90secTrg.Q = false;
	check90secTrg.PRE = false;

	heatShakerOnTON.IN = false;
	heatShakerOnTON.PRE = false;
	heatShakerOnTON.Q = false;
	heatShakerOnTON.PT = controller.timer8Interval * 1000;
	heatShakerOnTON.ET = 0;

	heatShakerOffTON.IN = false;
	heatShakerOffTON.PRE = false;
	heatShakerOffTON.Q = false;
	heatShakerOffTON.PT = controller.timer9Interval * 1000;
	heatShakerOffTON.ET = 0;

	checkTemp2TON.IN = false;
	checkTemp2TON.PRE = false;
	checkTemp2TON.Q = false;
	checkTemp2TON.PT = 300 * 1000;
	checkTemp2TON.ET = 0;

	heatVacFillerTP1.IN = false;
	heatVacFillerTP1.PRE = false;
	heatVacFillerTP1.Q = false;
	heatVacFillerTP1.PT = controller.timer7Interval * 1000;
	heatVacFillerTP1.ET = 0;

	heatVacFillerTP2.IN = false;
	heatVacFillerTP2.PRE = false;
	heatVacFillerTP2.Q = false;
	heatVacFillerTP2.PT = controller.timer7Interval * 1000;
	heatVacFillerTP2.ET = 0;

	heatVacFillerTrg1.IN = false;
	heatVacFillerTrg1.PRE = false;
	heatVacFillerTrg1.Q = false;

	heatVacFillerTrg2.IN = false;
	heatVacFillerTrg2.PRE = false;
	heatVacFillerTrg2.Q = false;

	heatPhotoErrTON.IN = false;
	heatPhotoErrTON.PRE = false;
	heatPhotoErrTON.Q = false;
	heatPhotoErrTON.PT = 5000;
	heatPhotoErrTON.ET = 0;
}

void heatCycleUpdate()
{

	// check io error
	if (checkIOError())
	{
		Serial.println(F("system has io error !"));
		controlFSM.transitionTo(standby);
		return;
	}

	// R2 ON
	controller.output.Relays.exhaustFan = true;

	// R1 On if T3 is greater than 100.0F
	if (controller.convectionTemp >= 100.0F)
		controller.output.Relays.blowerFan = true;
	else // R1 Off if T3 is lower than 100.0F
		controller.output.Relays.blowerFan = false;

	// Feeder Calculation cycle
	// Feeder on for timer5
	heatFeederOnTON.IN = ~heatFeederOffTON.Q;
	heatFeederOnTON.PT = controller.timer5Interval * 1000;
	TPFunc(&heatFeederOnTON);

	// Feeder off for timer6
	heatFeederOffTON.IN = ~heatFeederOnTON.Q;
	TPFunc(&heatFeederOffTON);
	controller.output.Relays.feeder = heatFeederOnTON.Q;

	// Temp2 <= 160.0F for 5min,
	checkTemp2TON.IN = (controller.waterTemp <= 150.0);
	TONFunc(&checkTemp2TON);

	// 90sec check timer
	check90secTON.IN = ~check90secTON.Q;
	TONFunc(&check90secTON);
	check90secTrg.IN = check90secTON.Q;
	RTrgFunc(&check90secTrg);
	if (check90secTrg.Q)
	{

		// We can confirm that wait a minute. ok i was think count down for checkTemp2TON.pt
		// now 90sec timer is executed once, it will perform go ahead
		Serial.println("90 sec timer triggered");
		// If (Temp2 <= 160F for 5 minutes), Timer6 + 1.5sec
		if (checkTemp2TON.Q)
		{
			if (heatFeederOffTON.PT > 1500)
				heatFeederOffTON.PT = heatFeederOffTON.PT + 1500;
		}

		// If Temp2 >= 160F Timer6 - 1.5sec
		if (controller.waterTemp >= 160.0F)
			heatFeederOffTON.PT = heatFeederOffTON.PT - 1500;
		// it will be displayed every 90sec
		Serial.print("Current Timer 6 is ");
		Serial.print(heatFeederOffTON.PT);
		Serial.println("ms.");
	}

	// if Temp2 >= 185.0F, over temp error
	if (controller.waterTemp >= 185.0F)
	{
		controller.fault.errors.overTempHeat = true;
		controlFSM.transitionTo(stopCycle);
		return;
	}

	// this part is for buildup shaker
	// Timer 8 . on, Timer 9 off, cycle
	heatShakerOnTON.IN = ~heatShakerOffTON.Q;
	heatShakerOnTON.PT = controller.timer8Interval * 1000;
	TPFunc(&heatShakerOnTON);

	heatShakerOffTON.IN = ~heatShakerOnTON.Q;
	heatShakerOffTON.PT = controller.timer9Interval * 1000;
	TPFunc(&heatShakerOffTON);
	controller.output.Relays.buildupShaker = heatShakerOnTON.Q;

	// check photosensor level in heat cycle, if error, goto start cycle (add timer here for 5 sec)
	heatPhotoErrTON.IN = controller.input.Switches.photoSensor == PHOTOERRORLEVEL;
	TONFunc(&heatPhotoErrTON);
	if (heatPhotoErrTON.Q)
		controlFSM.transitionTo(startCycle);

	// check door switch error, if error got stop cycle (add timer here for 5 sec)
	if (controller.input.Switches.doorSwitch == DOORSWITCHERRORLEVEL)
	{
		controller.fault.errors.doorErrHeat = true;
		controlFSM.transitionTo(stopCycle);
	}

	// if s6 off, then stop cycle
	if (controller.input.Switches.waterLow == WATERLOWERRORLEVEL)
	{
		controller.fault.errors.waterLowErr = true;
		controlFSM.transitionTo(stopCycle);
	}

	// If Pellets S5 opened, then r4 on for time 7, after 2 tries goto stop cycle
	/**************************************************************************************/
	heatVacFillerTP1.IN = (heatVacFillerTP2.Q == false) && (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL);
	heatVacFillerTP1.PT = controller.timer7Interval * 1000;
	TPFunc(&heatVacFillerTP1);

	heatVacFillerTrg1.IN = heatVacFillerTP1.Q;
	FTrgFunc(&heatVacFillerTrg1);
	if (heatVacFillerTrg1.Q)
	{
		Serial.println("first timer is expired");
	}

	heatVacFillerTP2.IN = heatVacFillerTrg1.Q && (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL);
	heatVacFillerTP2.PT = controller.timer7Interval * 1000;
	TPFunc(&heatVacFillerTP2);

	heatVacFillerTrg2.IN = heatVacFillerTP2.Q;
	FTrgFunc(&heatVacFillerTrg2);
	if (heatVacFillerTrg2.Q && (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL))
	{
		Serial.println("second timer is expired");
		controller.fault.errors.emptyPelletsErr = true;
		controlFSM.transitionTo(stopCycle);
	}

	controller.output.Relays.vacFiller = (heatVacFillerTP2.Q || heatVacFillerTP2.Q);
	/*************************************************************************************/
}

void stopCycleEnter()
{
	displayError();

	// reset outputs
	controller.output.data = 0x00;

	Serial.println(F("entered into stop cycle!"));

	stopFan1TON.IN = false;
	stopFan1TON.PRE = false;
	stopFan1TON.Q = false;
	stopFan1TON.PT = (controller.timer4Interval * 1000);
	stopFan1TON.ET = 0;

	stopPumpTON.IN = false;
	stopPumpTON.PRE = false;
	stopPumpTON.Q = false;
	stopPumpTON.PT = (controller.timer3Interval * 1000);
	stopPumpTON.ET = 0;

	stopVacFillerTP.IN = false;
	stopVacFillerTP.PRE = false;
	stopVacFillerTP.Q = false;
	stopVacFillerTP.PT = (controller.timer7Interval * 1000);
	stopVacFillerTP.ET = 0;
}

void stopCycleUpdate()
{
	if (checkIOError())
	{
		Serial.println(F("system has io error !"));
		controlFSM.transitionTo(standby);
		return;
	}

	// R2 (FAN2) ON when (TEMP 3 >= 100F)
	controller.output.Relays.exhaustFan = controller.convectionTemp >= 100.0F;

	// R3 (Feeder) OFF
	controller.output.Relays.feeder = false;

	// R4 (VAC) OFF
	controller.output.Relays.vacFiller = false;

	// R1 (FAN1) OFF after (Time4) && Temp3 < 100.0F
	stopFan1TON.IN = true;
	stopFan1TON.PT = (controller.timer4Interval * 1000);
	TONFunc(&stopFan1TON);
	if (stopFan1TON.Q && controller.convectionTemp < 100.0F)
		controller.output.Relays.blowerFan = false;
	else
		controller.output.Relays.blowerFan = true;

	// R5 (pump) OFF after (Time3) && Temp2 < 85.0F
	stopPumpTON.IN = true;
	stopPumpTON.PT = controller.timer3Interval * 1000;
	TONFunc(&stopPumpTON);
	if (stopPumpTON.Q && controller.waterTemp < 85.0F)
		controller.output.Relays.pump = false;
	else
		controller.output.Relays.pump = true;

	// If Pellets Empty S5 off, then r4 on for time 7
	stopVacFillerTP.IN = (controller.input.Switches.emptyPellets == EMPTYPELLETERRORLEVEL);
	controller.output.Relays.vacFiller = stopVacFillerTP.Q;

	// after pump, fan2, fan1, vacFiller are offed, program will enter into stanby status
	if (((controller.output.Relays.pump == false) && (controller.output.Relays.blowerFan == false)) &&
		((controller.output.Relays.exhaustFan == false) && (controller.output.Relays.vacFiller == false)))
	{
		controlFSM.transitionTo(standby);
	}
}

// ===================================================================================
// =          Enter into Sleep Mode                          =
// ===================================================================================
void EnterSleep()
{
	String cmd = "sleep=1";
	sendCommand(cmd.c_str());
	recvRetCommandFinished();
}

// ===================================================================================
// =          Exit from Sleep Mode                          =
// ===================================================================================
void ExitSleep()
{
	String cmd = "sleep=0";
	sendCommand(cmd.c_str());
	recvRetCommandFinished();
}

void onHeap(AsyncWebServerRequest *request)
{
	request->send(200, "text/plain", String(ESP.getFreeHeap()));
}

void onGetSetting(AsyncWebServerRequest *request)
{
	StaticJsonBuffer<256> jsonBuffer;

	JsonObject &root = jsonBuffer.createObject();
	root["watertempSetpoint"] = String(controller.tempSetting, 2);
	JsonArray &timers = root.createNestedArray("timersetting");
	timers.add(controller.timer1Interval);
	timers.add(controller.timer2Interval);
	timers.add(controller.timer3Interval);
	timers.add(controller.timer4Interval);
	timers.add(controller.timer5Interval);
	timers.add(controller.timer6Interval);
	timers.add(controller.timer7Interval);
	timers.add(controller.timer8Interval);
	timers.add(controller.timer9Interval);

	String strJSON;
	root.printTo(strJSON);
	request->send(200, "text/plain", strJSON);
}

void onGetStatus(AsyncWebServerRequest *request)
{
	StaticJsonBuffer<256> jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();

	if (controlFSM.isInState(standby))
		root["cycle"] = "Standby";
	else if (controlFSM.isInState(startCycle))
		root["cycle"] = "Starting cycle";
	else if (controlFSM.isInState(heatCycle))
		root["cycle"] = "Heating cycle";
	else if (controlFSM.isInState(stopCycle))
		root["cycle"] = "Stopping cycle";

	root["relay"] = controller.output.data;
	root["switch"] = controller.input.data;
	root["currerror"] = getCurrError();

	JsonArray &timers = root.createNestedArray("temphumi");
	timers.add(controller.pelletTemp);
	timers.add(controller.waterTemp);
	timers.add(controller.convectionTemp);
	timers.add(controller.exhaustTemp);
	timers.add(controller.humidity01);
	timers.add(controller.humidity02);

	String strJSON;
	root.printTo(strJSON);
	request->send(200, "text/plain", strJSON);
}

void onSettingTime(AsyncWebServerRequest *request)
{
	if (request->hasParam("timer1"))
	{
		AsyncWebParameter *p = request->getParam("timer1");
		controller.timer1Interval = p->value().toInt();
	}

	if (request->hasParam("timer2"))
	{
		AsyncWebParameter *p = request->getParam("timer2");
		controller.timer2Interval = p->value().toInt();
	}

	if (request->hasParam("timer3"))
	{
		AsyncWebParameter *p = request->getParam("timer3");
		controller.timer3Interval = p->value().toInt();
	}

	if (request->hasParam("timer4"))
	{
		AsyncWebParameter *p = request->getParam("timer4");
		controller.timer4Interval = p->value().toInt();
	}

	if (request->hasParam("timer5"))
	{
		AsyncWebParameter *p = request->getParam("timer5");
		controller.timer5Interval = p->value().toInt();
	}

	if (request->hasParam("timer6"))
	{
		AsyncWebParameter *p = request->getParam("timer6");
		controller.timer6Interval = p->value().toInt();
	}

	if (request->hasParam("timer7"))
	{
		AsyncWebParameter *p = request->getParam("timer7");
		controller.timer7Interval = p->value().toInt();
	}

	if (request->hasParam("timer8"))
	{
		AsyncWebParameter *p = request->getParam("timer8");
		controller.timer8Interval = p->value().toInt();
	}

	if (request->hasParam("timer9"))
	{
		AsyncWebParameter *p = request->getParam("timer9");
		controller.timer9Interval = p->value().toInt();
	}

	saveConfig();

	request->send(200, "text/plain", "OK");
}

void onRelayToggle(AsyncWebServerRequest *request)
{
	if (request->hasParam("chn"))
	{
		AsyncWebParameter *p = request->getParam("chn");
		switch (p->value().toInt())
		{
		case 1:
			controller.output.Relays.blowerFan = ~controller.output.Relays.blowerFan;
			break;
		case 2:
			controller.output.Relays.exhaustFan = ~controller.output.Relays.exhaustFan;
			break;
		case 3:
			controller.output.Relays.feeder = ~controller.output.Relays.feeder;
			break;
		case 4:
			controller.output.Relays.vacFiller = ~controller.output.Relays.vacFiller;
			break;
		case 5:
			controller.output.Relays.pump = ~controller.output.Relays.pump;
			break;
		case 6:
			controller.output.Relays.heater = ~controller.output.Relays.heater;
			break;
		case 7:
			controller.output.Relays.buildupShaker = ~controller.output.Relays.buildupShaker;
			break;
		}
	}
	request->send(200, "text/plain", "OK");
}

void onCycleToggle(AsyncWebServerRequest *request)
{
	if (controlFSM.isInState(standby))
	{
		// reset repeat counter
		nStartCycleRepeatCnt = 0;
		controlFSM.transitionTo(startCycle);
	}
	else if (controlFSM.isInState(startCycle) || controlFSM.isInState(heatCycle))
	{
		controlFSM.transitionTo(stopCycle);
	}
	request->send(200, "text/plain", "OK");
}

void onSaveTemp(AsyncWebServerRequest *request)
{
	if (request->hasParam("temp"))
	{
		AsyncWebParameter *p = request->getParam("temp");
		controller.tempSetting = p->value().toFloat();
		saveConfig();
	}
	request->send(200, "text/plain", "OK");
}
