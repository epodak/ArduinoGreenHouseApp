
/*
SPI bus: 11 (MOSI), 12(MISO), and 13(CLK)
SPI Ethernet CS: 10
SPI SD CS: 4
*/

#include <DHT.h>
#include <SD.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>
#include <avr/pgmspace.h> //flash, fast, 10,000x 
//#include <avr/eeprom.h>   //slow (3ms) [therefore use it only for startup variables], 100,000x 
#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef VLAD_COLLAPSE

/* ################# Hardware Settings ############################ */
/* address for clock on I2C bus */
#define DS3231_I2C_ADDRESS 0x68
#define outputEthernetPin 10
#define outputSdPin 4
#define oneWirePin 9
//#define outputLcdPin 8
#define SDISPEED SPI_HALF_SPEED
//  SPI_FULL_SPEED

#define DS18S20_ID 0x10
#define DS18B20_ID 0x28

/* ################# OneWire ############################ */
float temp;
//#define SHOWTEMP
#ifdef  SHOWTEMP
OneWire bus(oneWirePin);
DallasTemperature sensors(&bus);
DeviceAddress sensor1;
DeviceAddress sensor2;
#endif

/* ##################### temperature humidity ##################### */
#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
// Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHTTYPE);
// NOTE: For working with a faster chip, like an Arduino Due or Teensy, you
// might need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// Example to initialize DHT sensor for Arduino Due:
//DHT dht(DHTPIN, DHTTYPE, 30);

/* ##################### resetFunc ################################ */
void(*resetFunc) (void) = 0;  //declare reset function @ address 0


/* ##################### ETHERNET ################################# */
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 120 };
EthernetServer server(8080);
/* on API web client call, how much of the web request string we need to know how to respond: (GET /... )
should be the length of the largest api call (+1 for string end char \0):
GET /file/12345678.123 or
PUT /date or
U=15&M=12&d=15&h=23&m=55&s=15&w=2
A=pass1&*/
#define BUFSIZ 40
char request[BUFSIZ];
char putheader[BUFSIZ];

/* Our web server will timeout in this many ms */
//#define TIMEOUTMS 2000
/* ##################### SDCARD ################################### */
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

/* this variable is used for following operations:
datayyMM.jsn //has to be 8.3 format or it will error out!
YYYY-MM-dd HH:mm:ss // is 20 chars
*/
#define LOGFILENAMELENGTH 20 
char logFileName[LOGFILENAMELENGTH];
/* ##################### TIMER #################################### */
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

/*2015 or 05 or 22:
=> 4 chars + str end = 5 chars */
#define DATETIMECONVERSIONLENGTH 5
char dateTimeConversionValue[DATETIMECONVERSIONLENGTH];

byte lastLogTimeMin; //save minute of the last sensor log
byte lastLogTimeHour;
/* ##################### PROGMEM ################################## */
#ifndef VLAD_COLLAPSE
//for every copy of the program to the device it is writing to flash memmory (progmem) 
//therefore after we wrote this data once, we only reference it afterwards (thats why is commented)
const prog_char string_0[] PROGMEM = "0";   // "String 0" etc are strings to store - change to suit.
const prog_char string_1[] PROGMEM = "data";
const prog_char string_2[] PROGMEM = "err_";
const prog_char string_3[] PROGMEM = ".jsn";
const prog_char string_4[] PROGMEM = ".err";
const prog_char string_5[] PROGMEM = "session.txt";
const prog_char string_6[] PROGMEM = "log.txt";
const prog_char string_7[] PROGMEM = "settings.txt";
const prog_char string_8[] PROGMEM = "20";
const prog_char string_9[] PROGMEM = "-";
const prog_char string_10[] PROGMEM = ":";
const prog_char string_11[] PROGMEM = " ";

const prog_char string_12[] PROGMEM = "{";
const prog_char string_13[] PROGMEM = "\"";
const prog_char string_14[] PROGMEM = "\":\"";
const prog_char string_15[] PROGMEM = ",";
const prog_char string_16[] PROGMEM = "}";

PROGMEM const char *string_table[] = 	   // change "string_table" name to suit
{
	string_0,
	string_1,
	string_2,
	string_3,
	string_4,
	string_5,
	string_6,
	string_7,
	string_8,
	string_9,
	string_10,
	string_11,
	string_12,
	string_13,
	string_14,
	string_15,
	string_16
};

#endif
/* ##################### VARS ##################################### */

/* "
123456789012345678901234567890
{"SOmeFIeldName":"value", */
char outputResult[50];
//const char* day[] = { F("NotSet!"), F("Sun"), F("Mon"), F("Tue"), F("Wed"), F("Thu"), F("Fri"), F("Sat") };


byte session[7];     //save session start time here
byte sessionId[2];   //created session ID here

int minMaxRam[2]; //track max and min amout or ram used in program

int i; //for loops and general use.
/*
   0,1,2,3,4 - admin pin
   5 - log internet traffic
   6 - log frequency
   7 - log Ram Usage
   8 - boot if no SD
   */
const uint8_t settingsLength = 9;
unsigned char settings[settingsLength];


/*
analog from  -127 to 128    ??
digital from  0   to 1      ??
~digital from 0   to 1024   ??
12345678
-100,14
=> 4 chars + str end = 5 chars  */
//#define SENSORVALUELENGTH 5
char sensorValue[5];
//#define SENSORVALUELENGTH 10
char sensorFloatValue[10];


#endif

//#define SERIALDEBUG

void setup() {

	logInit();

	//sensorValue = (char*)malloc(sizeof(char) * SENSORVALUELENGTH);
	//dateTimeConversionValue = (char*)malloc(sizeof(char) * DATETIMECONVERSIONLENGTH);

	pinMode(outputEthernetPin, OUTPUT);                       // set the SS pin as an output (necessary!)
	digitalWrite(outputEthernetPin, HIGH);                    // but turn off the W5100 chip!
	//pinMode(53, OUTPUT);

	Wire.begin();

	SdInit();
	EthernetInit();

	sessionStart();

	initRamUsage(); //will set start amout of ram usage - min most likely

	dht.begin();
#ifdef  SHOWTEMP
	sensors.begin();
#endif
}

void loop()
{
	//hopefully web request is not longer than a minute 
	//so logger dont skip the time alloted to log the sensors
	//since it looks for exact minute value to do the log
	checkForApiRequests(); //check any API calls and respond 

	if (isTimeToLog()){ //check if minute is correct and if so - log sensor data.
		LogSensors(true, NULL);      //log sensor input to log file		
	}
	sessionTimeStamp(); //keep session running
	logRamUsage();
	delay(1000); //wait 1 sec
}

void SdInit(){
	if (!card.init(SDISPEED, outputSdPin)) cryticalError(1);
	if (!volume.init(&card)) cryticalError(1);
	if (!root.openRoot(&volume)) cryticalError(1);
}

void EthernetInit(){
	Ethernet.begin(mac, ip);
	server.begin();
}

/*######################## RAM LOGING ############################################*/
#ifndef VLAD_COLLAPSE
void initRamUsage()
{
	minMaxRam[0] = FreeRam();
	minMaxRam[1] = FreeRam();
}

void logRamUsage()
{
	if (settings[7] == '0') return;
	if (FreeRam() > minMaxRam[1]) {
		minMaxRam[1] = FreeRam();
	}
	if (FreeRam() < minMaxRam[0]) {
		minMaxRam[0] = FreeRam();
	}
}
#endif
/*##################### SESSION ##################################################*/
#ifndef VLAD_COLLAPSE
void sessionStart(){

	/* save curent date / time*/
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	session[0] = year;
	session[1] = month;
	session[2] = dayOfMonth;
	session[3] = hour;
	session[4] = minute;
	session[5] = second;
	session[6] = dayOfWeek;

	/* generate sessionID: here is logic how we create it: (customize on your will) */
	sessionId[0] = bcdToDec(session[5]);
	sessionId[1] = bcdToDec(session[4]);

	/* copy session file to log file for record of last session duration */
	SdFile fileTo;
	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[5]))); //'session.txt'
	if (file.open(&root, logFileName, O_READ)) {
		if (file.isFile()){
			strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
			if (fileTo.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
				if (fileTo.isFile()){
					int16_t c;
					logRamUsage();
					while ((c = file.read()) > 0) {
						fileTo.print((char)c);
					}
					//fileTo.println();
				}
				fileTo.close();
			} //end if fileTo.open
			else{
				cryticalError(1);
			}
		}
		file.close();
	} // end if file.open
	else{
		cryticalError(1);
	}
	/* done copying file */

	/* read setting file */
	readSettings();
}

/* constantly write to session file */
void sessionTimeStamp(){
	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[5]))); //'session.txt'
	if (file.open(&root, logFileName, O_CREAT | O_WRITE)) {
		if (file.isFile()){

			file.print(jsonField(true, "StartedTime", getStringDate(false), true, false));

			if (settings[7] == '1'){ //if logging ram?
				file.print(jsonField(false, "MinRam", showIntSensor(minMaxRam[0]), true, false));
				file.print(jsonField(false, "MaxRam", showIntSensor(minMaxRam[1]), true, false));
			}
			
			file.print(jsonField(false, "EndedTime", getStringDate(true), false, true));

		}
		file.close();
	}
	else{
		cryticalError(1);
	}
}

#endif
/*################# SETTINGS ####################################################**/
#ifndef VLAD_COLLAPSE
bool readSettings(){
	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[7]))); //'settings.txt
	if (file.open(&root, logFileName, O_READ)) {
		if (file.isFile()){
			int16_t c;
			int i = 0;
			logRamUsage();
			while ((c = file.read()) > 0) {
				if (i < settingsLength){
					settings[i] = c;
				}
				i++;
			}
			settings[settingsLength] = 0;
		}
		file.close();
		return true;
	}
	else{
		//default values:
		settings[0] = '1';
		settings[1] = '2';
		settings[2] = '3';
		settings[3] = '4';
		settings[4] = '5';//admin pass
		settings[5] = '0'; //log internet traffic
		settings[6] = '2'; //log frequency in minutes ( 0=10min, 1=30min, 2=60min)
		settings[7] = '1'; //keep log of free ram per session
		settings[8] = '1'; //Reboot if can not write to SD
		settings[settingsLength] = 0;   //terminate
		saveSettings();

	}
	return false;
}

void saveSettings(){

	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[7]))); //'settings.txt'
	if (file.open(&root, logFileName, O_CREAT | O_WRITE)) {
		if (file.isFile()){
			int i = 0;
			logRamUsage();
			while (i < settingsLength) {
				file.write(settings[i]); //write does ascii, while print does byte
				i++;
			}
		}
		file.close();
	}
	else{
		cryticalError(1);
	}

}

char* getSettings()
{
	settings[settingsLength] = 0;
	return (char*)settings;
}

bool isPassCorrect(char *pass)
{
#ifdef SERIALDEBUG
	Serial.print("isPassCorrect:");
	Serial.println(pass);
#endif
	//return true;
	//012345678 Header
	//PP: 12345
	//i = 3;
	return
		settings[0] == pass[0] &&
		settings[1] == pass[1] &&
		settings[2] == pass[2] &&
		settings[3] == pass[3] &&
		settings[4] == pass[4];

}
#endif
/*################# LOG SENSORS #################################################**/
#ifndef VLAD_COLLAPSE
/*
Log inputs to log file
- uses heap:
SdFile file;
const uint8_t numOfPins
uint8_t analogPin
uint8_t sensor
- uses stack:
sensorValue*

*/
void LogSensors(bool write, EthernetClient *client)
{

	//AnalogPins: 0,1,2,3 (4 & 5 are reserved for I2C so no need to log it)
	//const uint8_t numOfPins = 4;
	//int sensor = 0;  //int8_t not right
#ifdef  SHOWTEMP
	bool s1 = sensors.getAddress(sensor1, 0);
	bool s2 = sensors.getAddress(sensor2, 1);
	
	sensors.requestTemperatures();
#endif


	//bool found = getTemperature();
	if (write){
		if (file.open(root, getCurrentLogFileName(), O_CREAT | O_APPEND | O_WRITE))//Open or create the file
		{
			if (file.isFile()){

				file.print(jsonField(true, "datetime", getStringDate(true), true, false));

				//file.print(F("\", "));
				//for (uint8_t analogPin = 0; analogPin < numOfPins; analogPin++) {
				//	sensor = analogRead(analogPin);
				//	file.print(F("\"AnalogPin"));
				//	file.print(itoa(analogPin, sensorValue, 10));
				//	file.print(F("\": \""));
				//	file.print(itoa(sensor, sensorValue, 10));
				//	file.print(F("\""));
				//	if (analogPin < (numOfPins - 1)) {
				//		file.print(F(","));
				//	}
				//	logRamUsage();
				//}

				
				//if (found) file.print(jsonField(false, "W1TempC", showFloatSensor(temp), true, false));
#ifdef  SHOWTEMP
				if (s1) file.print(jsonField(false, "W1TempC", showFloatSensor(sensors.getTempC(sensor1)), true, false));
				if (s2) file.print(jsonField(false, "W2TempC", showFloatSensor(sensors.getTempC(sensor2)), true, false));
#endif
				file.print(jsonField(false, "TempC", showFloatSensor(dht.readTemperature()), true, false));
				file.print(jsonField(false, "HumPerc", showFloatSensor(dht.readHumidity()), false, true));

			}
			file.close();
		}
		else{
			cryticalError(1);
		}
	}
	else{

		printDateRamDetails(client);

#ifdef  SHOWTEMP
		if (s1) (*client).print(jsonField(false, "W1TempC", showFloatSensor(sensors.getTempC(sensor1)), true, false));
		if (s2) (*client).print(jsonField(false, "W2TempC", showFloatSensor(sensors.getTempC(sensor2)), true, false));
#endif
		(*client).print(jsonField(false, "TempC", showFloatSensor(dht.readTemperature()), true, false));
		(*client).print(jsonField(false, "HumPerc", showFloatSensor(dht.readHumidity()), false, true));
	}
}

/* log timer allows writting every X mins */
bool isTimeToLog()
{
	byte min;

	if (settings[6] == '0'){ min = 10; }     //log lines: 6/h, 144/d, 4320/month
	else if (settings[6] == '1'){ min = 30; }//log lines: 2/h, 48/d,  1488/month
	//else if (settings[6] == '2'){ min = 60; }//log lines: 1/h, 24/d,  744/month
	else { min = 60; } //default (anything > 59 wil log at only 0 mins since 0 % anything = 0!
	/* get current date: */
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	/* last log date is inside lastLogTime */
	if (
		(minute % min == 0) &&  //is time to log!
		(minute != lastLogTimeMin || lastLogTimeHour != hour)   //make sure we did not log this time already
		)
	{
		lastLogTimeMin = minute;   //mark as logged
		lastLogTimeHour = hour;
		return true;
	}
	else{
		return false;
	}

}

#endif
/* ################# API ########################################################**/
#ifndef VLAD_COLLAPSE

/*
char clientline[BUFSIZ];
uint8_t index
EthernetClient client
*/

void checkForApiRequests()
{
	EthernetClient client = server.available();
	if (client) {
		while (client.connected()) {
			if (client.available()) {

				ApiRequest_GetRequestDetails(&client, request, putheader);

				// If settings want us to log http traffic:
				if (settings[5] == '1') {
					logHttp(request, putheader);
				}
				/*****************************options response**********************************/
				if (strstr(request, "OPTIONS") != 0) {
					ApiRequest_GetOptionsScreen(&client);
				}
				/***************************** home page ***************************************/
				if (strcmp(request, "GET /") == 0) { //test if strings are equal
					if (strstr(putheader, "gzip") != 0)
						ApiRequest_GetFile(&client, "index.gz", NULL);
					else
						ApiRequest_GetErrorScreen(&client, false, false); // browser not supporting gzip. Alternatively return html?
				}
				/****************************** manifest file **********************************/
				else if (strstr(request, "GET /cache.app") != 0) { //test if strings contains
					ApiRequest_GetFile(&client, request + 5, NULL);
				}
				/****************************** login  (ANYONE) ********************************/
				else if (strstr(request, "GET /admin?U=") != 0) {
					ApiRequest_CheckLogInPin(&client, request + 13);
				}
				/****************************** sensors (ANYONE) *******************************/
				else if (strstr(request, "GET /status") != 0) {
					ApiRequest_ShowSensorStatus(&client);
				}
				/****************************** log file *************************/
				else if (strstr(request, "GET /data") != 0) {
					/* here you ask for data1501.jsn or data1502.jsn */
					ApiRequest_GetFile(&client, request + 5, NULL);
				}
				/****************************** system settings (ADMIN) ************************/
				else if (strstr(request, "GET /sysadmin") != 0) {
					ApiRequest_GetSystemSettings(&client, putheader);
				}
				/****************************** file content (ADMIN) ***************************/
				else if (strstr(request, "GET /file/") != 0) {
					ApiRequest_GetFile(&client, request + 10, putheader);
				}
				/***************************** list files  (ADMIN) *****************************/
				else if (strstr(request, "GET /files") != 0) {
					ApiRequest_GetFileList(&client, putheader);
				}
				/***************************** update settings  (ADMIN) ************************/
				else if (strstr(request, "PUT /settings") != 0) {
					//settings?U=111112345
					ApiRequest_PutSettings(&client, putheader, request + 13);
				}
				/***************************** update clock  (ADMIN) ***************************/
				else if (strstr(request, "PUT /clock?U=") != 0) {
					ApiRequest_PutDateTime(&client, putheader, request + 13);
				}
				/***************************** reboot  (ADMIN)  ********************************/
				else if (strstr(request, "PUT /reboot") != 0) {
					ApiRequest_PutReboot(&client, putheader);
				}
				/**********************************404******************************************/
				else {
					ApiRequest_GetErrorScreen(&client, true, false);
				}
				break;
			}
		}
		// give the web browser time to receive the data		
		delay(1);
		client.stop();
	}

}

void ApiRequest_GetRequestDetails(EthernetClient *client, char* request, char* putheader)
{
	/*  puts first line into "request" (it trims ' HTTP/1.1')

		puts into 'putheader': PP line
		- unless it is 'GET /' requests then puts 'Accept-Encoding' line.
		*/

	uint8_t index = 0;
	uint8_t lineNo = 1;
	int c = (*client).read();
	while (c >= 0){
		// If it isn't a new line, add the character to the buffer
		if (c != '\n' && c != '\r') {
			if (index < BUFSIZ - 1)
			{
				putheader[index] = c; //read current line
				index++;
			}
		}
		else{
			if (c == '\n'){
				putheader[index] = 0;
				index = 0;
				if (lineNo == 1){
					strcpy(request, putheader);
					(strstr(request, " HTTP"))[0] = 0; //trim junk
				}
				else if (
					//if requesting index, look for 'Accept-Encoding' line to ckeck gzip attribute
					((strcmp(request, "GET /") == 0) && (strstr(putheader, "Accept-Encoding: ") != 0))
					||
					//or when PP is found we are done!
					(strstr(putheader, "PP: ") != 0)
					)
				{
					index = BUFSIZ - 1; //stop reading
				}
				lineNo++;
			}
		}
		logRamUsage();
		c = (*client).read();
	}//end while

#ifdef SERIALDEBUG
	Serial.print("request: \'");
	Serial.print(request);
	Serial.println("\'");
	Serial.print("putheader: '");
	Serial.print(putheader);
	Serial.println("\'");
#endif
}

void ApiRequest_GetSuccessHeader(EthernetClient *client, char* filename)
{
	ApiRequest_HelpHttpOK(client);
	if (strstr(filename, ".jsn") != 0)
		ApiRequest_HelpContentTypeJson(client);
	else if (strstr(filename, ".app") != 0)
		(*client).println(F("Content-Type: text/cache-manifest"));
	else if (strstr(filename, ".gz") != 0){
		(*client).println(F("Content-Encoding: gzip"));
		ApiRequest_HelpContentTypeHtml(client);
	}
	else
		ApiRequest_HelpContentTypeHtml(client);


	ApiRequest_HelpAccessControllAllow(client, false);

	ApiRequest_HelpConnectionClose(client);
}

void ApiRequest_GetErrorScreen(EthernetClient *client, bool is404, bool isJson)
{
	if (is404){
		(*client).println(F("HTTP/1.1 404 Not Found"));
	}
	else{
		(*client).println(F("HTTP/1.1 401 Access Denied"));
	}

	if (isJson)
		ApiRequest_HelpContentTypeJson(client);
	else
		ApiRequest_HelpContentTypeHtml(client);

	ApiRequest_HelpAccessControllAllow(client, false);
	ApiRequest_HelpConnectionClose(client);

}

void ApiRequest_GetOptionsScreen(EthernetClient *client)
{

	ApiRequest_HelpHttpOK(client);
	ApiRequest_HelpAccessControllAllow(client, true);
	ApiRequest_HelpConnectionClose(client);
}

void ApiRequest_ShowSensorStatus(EthernetClient *client){
	ApiRequest_GetSuccessHeader(client, ".jsn");
	LogSensors(false, client);
}

void ApiRequest_CheckLogInPin(EthernetClient *client, char* pass){

	if (isPassCorrect(pass))
	{
		ApiRequest_GetSuccessHeader(client, ".jsn");
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

void ApiRequest_GetFileList(EthernetClient *client, char* putheader){

	if (isPassCorrect(putheader + 4))
	{
		ApiRequest_GetSuccessHeader(client, ".jsn");
		ListFiles(client, LS_SIZE); //LS_SIZE 
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}

}

void ApiRequest_GetFile(EthernetClient *client, char* filename, char* putheader)
{
	if (
		(putheader != NULL && isPassCorrect(putheader + 4)) ||  //if admin
		(putheader == NULL &&
		(
		strstr(filename, "data") ||        //or asking data file
		strstr(filename, "cache.app") ||   //or asking cache file
		strstr(filename, "index.gz")       //or asking index file
		)
		)

		)
	{

		if (file.open(&root, filename, O_READ)) {
			ApiRequest_GetSuccessHeader(client, filename);
			int16_t c;
			while ((c = file.read()) >= 0) {
				(*client).print((char)c);
			}
			file.close();
		}
		else{
			ApiRequest_GetErrorScreen(client, true, false);
		}
	}
	else{
		ApiRequest_GetErrorScreen(client, false, false);
	}
}

void ApiRequest_GetSystemSettings(EthernetClient *client, char* putheader)
{
	if (isPassCorrect(putheader + 4))
	{
		ApiRequest_GetSuccessHeader(client, ".jsn");
		printDateRamDetails(client);
		(*client).println(jsonField(false, "Settings", getSettings(), false, true));

	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}

}

void ApiRequest_PutSettings(EthernetClient *client, char* putheader, char* parameters)
{
	//012345678901
	//?U=123456789
	if (isPassCorrect(putheader + 4))
	{
		for (i = 0; i < 9; i++){//settings has 9 fields (1-5 admin + 
			settings[i] = parameters[i + 3];
		}
		saveSettings();

		ApiRequest_GetSuccessHeader(client, ".jsn");
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

void ApiRequest_PutDateTime(EthernetClient *client, char* putheader, char* parameters)
{
	//          10        20
	//012345678901234567890 
	//D=YYMMDDhhmmssw

	//to update clock do:
	//setDS3231time(15, 2, 10, 23, 43, 10, 3);  //2015-05-22 21:15:30 tuesday
	if (isPassCorrect(putheader + 4))
	{
		/* should we check here if all numbers are between 0 amd 9, otherwise error out? */

		setDS3231time(
			toDec(parameters[0], parameters[1]), //y
			toDec(parameters[2], parameters[3]), //m
			toDec(parameters[4], parameters[5]), //d
			toDec(parameters[6], parameters[7]), //h
			toDec(parameters[8], parameters[9]), //m
			toDec(parameters[10], parameters[11]), //s
			toDec(parameters[12])                 //w			
			);
		ApiRequest_GetSuccessHeader(client, ".jsn");
		(*client).print(jsonField(true, "newdate", getStringDate(true), false, true));
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

void ApiRequest_PutReboot(EthernetClient *client, char* arguments)
{
	if (isPassCorrect(arguments + 4))
	{
		ApiRequest_GetSuccessHeader(client, ".jsn");
		delay(1);
		(*client).stop();
		delay(15 * 1000); // wait 15 sec so user can remove sd card safelly
		resetFunc();
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}

}

void ApiRequest_HelpHttpOK(EthernetClient *client)
{
	(*client).println(F("HTTP/1.1 200 OK"));
}

void ApiRequest_HelpConnectionClose(EthernetClient *client)
{
	(*client).println(F("Connection: close"));
	(*client).println();
}

void ApiRequest_HelpAccessControllAllow(EthernetClient *client, bool option)
{
	//if (true){ //if settings allow other javascript websites to query our api
		if (option) (*client).println(F("Allow: GET, PUT, POST, OPTIONS"));
		(*client).println(F("Access-Control-Allow-Origin: *"));
		(*client).println(F("Access-Control-Allow-Headers: PP"));
		if (option) (*client).println(F("Access-Control-Allow-Methods: GET, PUT, POST, OPTIONS"));
	//}
}

void ApiRequest_HelpContentTypeHtml(EthernetClient *client)
{
	(*client).println(F("Content-Type: text/html"));
}

void ApiRequest_HelpContentTypeJson(EthernetClient *client)
{
	(*client).println(F("Content-Type: application/json"));
}

byte toDec(char A, char B)
{
	/* convert each ASCII to byte value and add them
	Example:
	VAR  A     ,B
	ASCI 1      5
	DEC: 49    ,53
	BIN: 110001,110101
	*/
	return (toDec(A) * 10 + toDec(B));
}
byte toDec(char A)
{
	return A - '0';
}

#endif
/*############################ List FIles ########################################*/

void ListFiles(EthernetClient *client, uint8_t flags) {
	// This code is just copied from SdFile.cpp in the SDFat library
	// and tweaked to print to the client output in html!
	dir_t p;
	bool first = true;
	root.rewind();
	(*client).print(F("{"));
	while (root.readDir(p) > 0) {
		// done if past last used entry
		if (p.name[0] == DIR_NAME_FREE) break;

		// skip deleted entry and entries for . and  ..
		if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

		// only list subdirectories and files
		if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

		// print any indent spaces
		if (first){
			first = false;
		}
		else{
			(*client).print(F(",")); //skip first time!
		}
		(*client).print(F("\""));
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				(*client).print(F("."));
			}
			(*client).print((char)p.name[i]);
		}
		(*client).print(F("\":"));

		// print size if requested
		if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
			(*client).print(p.fileSize);
		}
	}
	(*client).print(F("}"));

}

/*############################ Clock #############################################*/
#ifndef VLAD_COLLAPSE


// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
	return((val / 10 * 16) + (val % 10));
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
	return((val / 16 * 10) + (val % 16));
}

//setDS3231time(15,5,22,21,15,30,  6);  //2015-05-22 21:15:30 friday
void setDS3231time(byte  year, byte  month, byte  dayOfMonth, byte  hour, byte  minute, byte  second, byte  dayOfWeek)
{
	// sets time and date data to DS3231
	Wire.beginTransmission(DS3231_I2C_ADDRESS);
	Wire.write(0); // set next input to start at the seconds register
	Wire.write(decToBcd(second)); // set seconds
	Wire.write(decToBcd(minute)); // set minutes
	Wire.write(decToBcd(hour)); // set hours
	Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
	Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
	Wire.write(decToBcd(month)); // set month
	Wire.write(decToBcd(year)); // set year (0 to 99)
	Wire.endTransmission();
}
void readDS3231time(byte *year, byte *month, byte *dayOfMonth, byte *hour, byte *minute, byte *second, byte *dayOfWeek)
{
	Wire.beginTransmission(DS3231_I2C_ADDRESS);
	Wire.write(0); // set DS3231 register pointer to 00h
	Wire.endTransmission();
	Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
	// request seven bytes of data from DS3231 starting from register 00h
	*second = bcdToDec(Wire.read() & 0x7f);
	*minute = bcdToDec(Wire.read());
	*hour = bcdToDec(Wire.read() & 0x3f);
	*dayOfWeek = bcdToDec(Wire.read());
	*dayOfMonth = bcdToDec(Wire.read());
	*month = bcdToDec(Wire.read());
	*year = bcdToDec(Wire.read());
}

//print military time as 2000-05-22 13:33:21
char* getStringDate(bool currentTime)
{

	if (currentTime){
		//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
		// retrieve data from DS3231
		readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	}
	else{
		//get time of the session start
		year = session[0];
		month = session[1];
		dayOfMonth = session[2];
		hour = session[3];
		minute = session[4];
		second = session[5];
		dayOfWeek = session[6];
	}

	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[8]))); //"20"
	appendNumber(year, 9); //"-"
	appendNumber(month, 9); //"-"
	appendNumber(dayOfMonth, 11); //" "
	appendNumber(hour, 10); //":"
	appendNumber(minute, 10); //":"
	appendNumber(second, NULL);

	//strcat_P(datetimestring, (char*)pgm_read_word(&(string_table[11]))); //" "
	//strcat_P(datetimestring, itoa(dayOfWeek, dateTimeConversionValue, DEC)); //'dayOfWeek'

	return logFileName;
}

void appendNumber(byte number, int postfix) // 9 = "-", 11 = " ", 10 = ":"
{
	if (number < 10)
	{
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[0]))); //"0"
	}
	strcat(logFileName, itoa(number, dateTimeConversionValue, DEC));
	if (postfix != NULL){
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[postfix])));
	}
}

char* getCurrentLogFileName(){

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor


	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[1]))); //'data'
	appendNumber(year, NULL);
	appendNumber(month, NULL);
	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[3]))); //'.jsn'

	return logFileName;
}

//char* getCurrentErrorFileName(){
//
//	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
//	// retrieve data from DS3231
//	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
//	// send it to the serial monitor
//
//	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[2]))); //'err_'
//	appendNumber(year, NULL);
//	appendNumber(month, NULL);
//	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[4]))); //'.err'
//
//	return logFileName;
//}

#endif

/*############################ Temperature #######################################*/
#ifndef VLAD_COLLAPSE

//
//boolean getTemperature(){
//	byte i;
//	byte present = 0;
//	byte data[12];
//	byte addr[8];
//	//find a device
//	//if (!ds.search(addr)) {
//	//	ds.reset_search();
//	//	return false;
//	//}
//	if (OneWire::crc8(addr, 7) != addr[7]) {
//		return false;
//	}
//	if (addr[0] != DS18S20_ID && addr[0] != DS18B20_ID) {
//		return false;
//	}
//	ds.reset();
//	ds.select(addr);
//	// Start conversion
//	ds.write(0x44, 1);
//	// Wait some time...
//	delay(850);
//	present = ds.reset();
//	ds.select(addr);
//	// Issue Read scratchpad command
//	ds.write(0xBE);
//	// Receive 9 bytes
//	for (i = 0; i < 9; i++) {
//		data[i] = ds.read();
//	}
//	// Calculate temperature value
//	temp = ((data[1] << 8) + data[0])*0.0625;
//	return true;
//	}
#endif
/*############################ Loging ############################################*/
#ifndef VLAD_COLLAPSE
void logInit(){
#ifdef SERIALDEBUG
	Serial.begin(9600);
#endif
}

void serialShow(char* line){
#ifdef SERIALDEBUG
	Serial.println(line);
#endif
}

void logHttp(char* line, char* args){

	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
	if (file.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
		if (file.isFile()){
			file.print(jsonField(true, "HTTP", line, true, false));
			file.print(jsonField(false, "params", args, true, false));
			file.print(jsonField(false, "datetime", getStringDate(true), false, true));
}
		file.close();
	}
}

//void log(char* line){
//
//	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
//	if (file.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
//		if (file.isFile()){
//			file.print(F("{ \"debug\":\""));
//			file.print(line);
//			file.print(F("\", \"datetime\":\""));
//			printCurrentStringDateToFile(&file, true);
//			file.print(F("\" }"));
//			file.println();
//		}
//		file.close();
//	}
//}

void cryticalError(byte Reason){
	//1 - SD card error - can not open file

	if (Reason == 1 && settings[8] == '1') //if option to reboot on SD error - then reboot
	{
#ifdef SERIALDEBUG
		Serial.println("booting....");
#endif
		//Can not write error to any file because this is SD error.... write to eeprom?
		resetFunc();  //call reset
	}
}
#endif
/*########################### Helpers ############################################*/
#ifndef VLAD_COLLAPSE


//char* showFloatSensorN(float fsensor){
//	int res = sprintf(sensorFloatValue, "%.2f", fsensor);
//	if (res < 0) return "NA";
//	else return sensorFloatValue;
//}
//
//char* showIntSensorN(int isensor){
//	int res = sprintf(sensorValue, "%i", isensor);
//	if (res < 0) return "NA";
//	else return sensorValue;
//}

char* showFloatSensor(float f)
{
	if (isnan(f)) return "NA";

	int whole = (int)((float)f);
	int dec = (int)(((float)f - whole) * 100);

	strcpy(sensorFloatValue, showIntSensor(whole));
	strcat(sensorFloatValue, ".");
	if (dec < 10) strcat(sensorFloatValue, "0");
	strcat(sensorFloatValue, showIntSensor(dec));
	return sensorFloatValue;
}

char* showIntSensor(int sensor){

	int i, rem;

	int n = sensor;
	int len = 0;
	while (n != 0)
	{
		len++;
		n /= 10;
	}

	n = sensor;
	for (i = 0; i < len; i++)
	{
		rem = n % 10;
		n = n / 10;
		sensorValue[len - (i + 1)] = rem + '0';
	}
	sensorValue[len] = 0; // '\0';
	return sensorValue;
}

char* jsonField(bool isStart, char* str, char* val, bool postfix, bool isEnd){
	// {"str":"val", ... }

	strcpy(outputResult, "");
	if (isStart) strcat_P(outputResult, (char*)pgm_read_word(&(string_table[12]))); // {
	strcat_P(outputResult, (char*)pgm_read_word(&(string_table[13]))); // "
	strcat(outputResult, str);
	strcat_P(outputResult, (char*)pgm_read_word(&(string_table[14]))); // ":"
	strcat(outputResult, val);
	strcat_P(outputResult, (char*)pgm_read_word(&(string_table[13]))); // "
	if (postfix) strcat_P(outputResult, (char*)pgm_read_word(&(string_table[15]))); // ,
	if (isEnd) {
		strcat_P(outputResult, (char*)pgm_read_word(&(string_table[16]))); // }
		strcat(outputResult, "\n");
	}

	return outputResult;
}

void printDateRamDetails(EthernetClient *client)
{
	(*client).print(jsonField(true, "DeviceTime", getStringDate(true), true, false));
	(*client).print(jsonField(false, "MinRam", showIntSensor(minMaxRam[0]), true, false));
	(*client).print(jsonField(false, "MaxRam", showIntSensor(minMaxRam[1]), true, false));
	(*client).print(jsonField(false, "RunningSince", getStringDate(false), true, false));
}

#endif

/*################################################################################*/
