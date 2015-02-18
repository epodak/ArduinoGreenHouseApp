
/*
SPI bus: 11 (MOSI), 12(MISO), and 13(CLK)
SPI Ethernet CS: 10
SPI SD CS: 4
*/

#include <SD.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>
#include <avr/pgmspace.h> //flash, fast, 10,000x 
#include <avr/eeprom.h>   //slow (3ms) [therefore use it only for startup variables], 100,000x 

/* ################# Hardware Settings ############################ */
/* address for clock on I2C bus */
#define DS3231_I2C_ADDRESS 0x68
#define outputEthernetPin 10
#define outputSdPin 4
#define outputLcdPin 8
#define SDISPEED SPI_HALF_SPEED

void(*resetFunc) (void) = 0;  //declare reset function @ address 0

/************ ETHERNET STUFF ************/
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 120 };
EthernetServer server(8080);

/************ SDCARD STUFF ************/
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

/************ TIMER STUFF *************/
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

/************ PROGMEM STUFF *************/
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
PROGMEM const char *string_table[] = 	   // change "string_table" name to suit
{
	string_0,
	string_1,
	string_2,
	string_3,
	string_4,
	string_5,
	string_6,
	string_7
};

/************ DYNAMIC VARS ************/
/*
datayyMM.jsn //has to be 8.3 format or it will error out! */
#define LOGFILENAMELENGTH 13 
char logFileName[LOGFILENAMELENGTH];
char* sensorValue;
char* dateTimeConversionValue;
const char* day[] = { "NotSet!", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

byte lastLogTimeMin; //save minute of the last sensor log
byte session[7];     //save session start time here
byte sessionId[2];   //created session ID here

int minMaxRam[2]; //track max and min amout or ram used in program


/*
   0,1,2,3,4 - admin pin
   5 - log internet traffic
   6 - log frequency
   7 - log Ram Usage
   */
const uint8_t settingsLength = 8;
unsigned char settings[settingsLength];


/*
analog from  -127 to 128    ??
digital from  0   to 1      ??
~digital from 0   to 1024   ??
=> 4 chars + str end = 5 chars  */
#define SENSORVALUELENGTH 5
/*2015 or 05 or 22:
=> 4 chars + str end = 5 chars */
#define DATETIMECONVERSIONLENGTH 5
/* on API web client call, how much of the web request string we need to know how to respond: (GET /... )
   should be the length of the largest api call (+1 for string end char \0):
   GET /file/12345678.123 or
   PUT /date or
   A=pass1&y=15&M=12&d=15&h=23&m=55&s=15&w=2  */
#define BUFSIZ 30
/* Our web server will timeout in this many ms */
//#define TIMEOUTMS 2000

// store error strings in flash to save RAM
//#define error(s) error_P(PSTR(s))
//
//void error_P(const char* str) {
//	PgmPrint("error: ");
//	SerialPrintln_P(str);
//	if (card.errorCode()) {
//		PgmPrint("SD error: ");
//		Serial.print(card.errorCode(), HEX);
//		Serial.print(',');
//		Serial.println(card.errorData(), HEX);
//	}
//	while (1);
//}

void setup() {

	logInit();

	sensorValue = (char*)malloc(sizeof(char) * SENSORVALUELENGTH);
	dateTimeConversionValue = (char*)malloc(sizeof(char) * DATETIMECONVERSIONLENGTH);

	pinMode(outputEthernetPin, OUTPUT);                       // set the SS pin as an output (necessary!)
	digitalWrite(outputEthernetPin, HIGH);                    // but turn off the W5100 chip!

	Wire.begin();

	SdInit();
	EthernetInit();

	sessionStart();

	initRamUsage(); //will set start amout of ram usage - min most likely

}

void loop()
{
	//hopefully web request is not longer than a minute 
	//so logger dont skip the time alloted to log the sensors
	//since it looks for exact minute value to do the log
	checkForApiRequests(); //check any API calls and respond 

	if (isTimeToLog()){ //check if minute is correct and if so - log sensor data.
		LogSensors();      //log sensor input to log file		
	}
	sessionTimeStamp(); //keep session running
	logRamUsage();
	delay(1000); //wait 1 sec
}

void SdInit(){
	if (!card.init(SDISPEED, outputSdPin)) cryticalError();
	if (!volume.init(&card)) cryticalError();
	if (!root.openRoot(&volume)) cryticalError();
}

void EthernetInit(){
	Ethernet.begin(mac, ip);
	server.begin();
}

/*######################## RAM LOGING ##################################*/

void initRamUsage()
{
	minMaxRam[0] = FreeRam();
	minMaxRam[1] = FreeRam();
}

void logRamUsage()
{
	if (settings[7] == 0) return;
	if (FreeRam() > minMaxRam[1]) {
		minMaxRam[1] = FreeRam();
	}
	if (FreeRam() < minMaxRam[0]) {
		minMaxRam[0] = FreeRam();
	}
}

/*##################### SESSION #########################################*/

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
					fileTo.println();
				}
				fileTo.close();
			} //end if fileTo.open
			else{
				cryticalError();
			}
		}
		file.close();
	} // end if file.open
	else{
		cryticalError();
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
			file.print(F("{ \"Session\":\""));
			file.print(sessionId[0], DEC);
			file.print(sessionId[1], DEC);
			file.print(F("\", "));
			file.print(F("\"StartedTime\":\""));
			printCurrentStringDateToFile(&file, false); //print session start time from session ID
			file.print(F("\", "));
			file.print(F("\"EndedTime\":\""));
			printCurrentStringDateToFile(&file, true); //print current time
			file.print(F("\" }                            ")); //space to delete trailing chars from previous session

		}
		file.close();
	}
	else{
		cryticalError();
	}
}

/* log timer allows writting every X mins */
bool isTimeToLog()
{
	byte min;

	if (settings[6] == '0'){ min = 10; }     //log lines: 6/h, 144/d, 4320/month
	else if (settings[6] == '1'){ min = 30; }//log lines: 2/h, 48/d,  1488/month
	else if (settings[6] == '2'){ min = 60; }//log lines: 1/h, 24/d,  744/month
	else { min = 60; } //default
	/* get current date: */
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	/* last log date is inside lastLogTime */
	if (
		(minute % min == 0) &&       //is time to log!
		(minute != lastLogTimeMin)   //make sure we did not log this time already
		)
	{
		lastLogTimeMin = minute;   //mark as logged
		return true;
	}
	else{
		return false;
	}

}

/*################# SETTINGS ####################################**/
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
		settings[5] = '1'; //log internet traffic
		settings[6] = '1'; //log frequency in minutes ( 0=10min, 1=30min, 2=60min)
		settings[7] = '1'; //keep log of free ram per session
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
				file.write(settings[i]);
				i++;
			}
		}
		file.print("                      ");
		file.close();
	}
	else{
		cryticalError();
	}

}

bool isPassCorrect(char *pass)
{
	if (
		settings[0] == pass[0] &&
		settings[1] == pass[1] &&
		settings[2] == pass[2] &&
		settings[3] == pass[3] &&
		settings[4] == pass[4]
		)
		return true;
	else
		return false;
}

/*################# LOG SENSORS ####################################**/

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
void LogSensors()
{

	//AnalogPins: 0,1,2,3 (4 & 5 are reserved for I2C so no need to log it)
	const uint8_t numOfPins = 4;
	int sensor = 0;  //int8_t not right

	if (file.open(root, getCurrentLogFileName(), O_CREAT | O_APPEND | O_WRITE))//Open or create the file
	{
		if (file.isFile()){

			file.print(F("{ \"datetime\":\""));
			printCurrentStringDateToFile(&file, true);
			file.print(F("\", "));

			file.print(F(" \"Id\":\""));
			file.print(sessionId[0], DEC);
			file.print(sessionId[1], DEC);
			file.print(F("\", "));

			//file.print(F(" \"lastboot\":\""));
			//printCurrentStringDateToFile(&file, false);
			//file.print(F("\", "));

			//file.print(F(" \"freeRamInBytes\":\""));
			//file.print(FreeRam());
			//file.print(F("\", "));

			for (uint8_t analogPin = 0; analogPin < numOfPins; analogPin++) {
				sensor = analogRead(analogPin);
				file.print(F("\"AnalogPin"));
				file.print(itoa(analogPin, sensorValue, 10));
				file.print(F("\": \""));
				file.print(itoa(sensor, sensorValue, 10));
				file.print(F("\""));
				if (analogPin < (numOfPins - 1)) {
					file.print(F(","));
				}
				logRamUsage();
			}
			file.print(F("}"));
			file.println();
		}
		file.close();
	}
	else{
		cryticalError();
	}
}

/*################# API ####################################**/
/*
char clientline[BUFSIZ];
uint8_t index
EthernetClient client
*/

void checkForApiRequests()
{
	char clientline[BUFSIZ];
	char putargs[BUFSIZ];

	EthernetClient client = server.available();
	if (client) {
		while (client.connected()) {
			if (client.available()) {

				ApiRequest_GetRequestDetails(&client, clientline, putargs);

				// If settings want us to log http traffic:
				if (settings[5] == '1') {				
					logHttp(clientline, putargs);
				}				

				/*****************************home page*****************************************/
				if (strstr(clientline, "GET / ") != 0) {	
					ApiRequest_GetHomePage(&client);
				}
				/****************************** manifest file **********************************/
				else if (strstr(clientline, "GET /cache.appcache") != 0) {
					ApiRequest_GetIndividualFile(&client, clientline + 5);
				}
				/****************************** file content (ADMIN) ***************************/
				else if (strstr(clientline, "GET /file/") != 0) {
					ApiRequest_GetIndividualFile(&client, clientline + 10, putargs);
				}
				/***************************** list files  (ADMIN) *****************************/
				else if (strstr(clientline, "GET /files") != 0) {
					ApiRequest_GetFileList(&client, putargs);
				}
				/***************************** show clock **************************************/
				else if (strstr(clientline, "GET /clock") != 0) {
					ApiRequest_GetDateTime(&client);
				}
				/***************************** show ram ****************************************/
				else if (strstr(clientline, "GET /freeram") != 0) {
					ApiRequest_GetRam(&client);
				}
				/***************************** update settings  (ADMIN) ************************/
				else if (strstr(clientline, "PUT /settings") != 0) {
					ApiRequest_PutSettings(&client, putargs);
				}
				/***************************** update clock  (ADMIN) ***************************/
				else if (strstr(clientline, "PUT /clock") != 0) {
					ApiRequest_PutDateTime(&client, putargs);
				}
				/***************************** reboot  (ADMIN)  ********************************/
				else if (strstr(clientline, "PUT /reboot") != 0) {
					ApiRequest_PutReboot(&client, putargs);
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

void ApiRequest_GetRequestDetails(EthernetClient *client, char* request, char* params)
{
	uint8_t index = 0;
	uint8_t indexargs = 0;
	bool parametersFound = false;
	int c = (*client).read();
	params[0] = c;
	while (c > -1){
		// If it isn't a new line, add the character to the buffer
		if (c != '\n' && c != '\r') {
			/*---- find request ----------------------*/
			if (index  < BUFSIZ)
			{
				request[index] = c; //read first line
				index++;
			}
			/*---- find post or put arguments --------*/
			if (!parametersFound){ //look for A=
				params[0] = params[1];
				params[1] = c;
				if (params[0] == 'A' && params[1] == '='){
					parametersFound = true;
					indexargs=2;
				}
			}
			else{
				if (indexargs < BUFSIZ){					
					params[indexargs] = c;
					indexargs++;
				}
			}
			/*-----------------------------------------*/
			//continue; //restart while
		}
		else{
			if (index > 3 && index < BUFSIZ) {
				request[index] = 0;
				index = BUFSIZ + 1;
			}
			if (indexargs > 3 && indexargs < BUFSIZ) {
				params[indexargs] = 0;
				indexargs = BUFSIZ + 1;
			}			
		}
		logRamUsage();
		c = (*client).read();
	}//end while
	//folowing just prevents any bug if request line is very long 
	//so we did not have chance to break the line yet:
	params[BUFSIZ-1] = 0;
	params[BUFSIZ-1] = 0;
}

void ApiRequest_GetSuccessHeader(EthernetClient *client, char* filename)
{
	(*client).println(F("HTTP/1.1 200 OK"));
	if (strstr(filename, ".JSN") != 0)
		(*client).println(F("Content-Type: application/json"));
	else if (strstr(filename, ".CHE") != 0)
		(*client).println(F("Content-Type: text/cache-manifest"));
	else
		(*client).println(F("Content-Type: text/html"));
	(*client).println(F("Connection: close"));
	(*client).println();
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
		(*client).println(F("Content-Type: application/json"));
	else
		(*client).println(F("Content-Type: text/html"));

	(*client).println(F("Connection: close"));
	(*client).println();
	if (is404){
		if (isJson)
			(*client).println(F("{ \"error\":\"404 - File Not Found\" }"));
		else
			(*client).println(F("<h2>File Not Found!</h2>"));
	}
	else{
		if (isJson)
			(*client).println(F("{ \"error\":\"401 - Access Denied\" }"));
		else
			(*client).println(F("<h2>Access Denied</h2>"));
	}
	(*client).println();
}

void ApiRequest_GetHomePage(EthernetClient *client)
{
	if (file.open(&root, "INDEX.HTM", O_READ)) {
		ApiRequest_GetSuccessHeader(client, false);
		int16_t c;
		while ((c = file.read()) > 0) {
			(*client).print((char)c);
		}
		file.close();
	}
	else{
		ApiRequest_GetErrorScreen(client, true, false);
		cryticalError(); ///!!!!!!!!!!!!		
	}
}

void ApiRequest_GetFileList(EthernetClient *client, char* arguments){

	/* in PUT request pass is passed as:
	01234567
	A=: 1234
	*/
	if (isPassCorrect(&arguments[4]))
	{
		ApiRequest_GetSuccessHeader(client, false);
		// print all the files, use a helper to keep it clean
		(*client).println(F("<h2>Files:</h2>"));
		ListFiles(client, LS_SIZE); //LS_SIZE | LS_DATE
	}
	else{
		ApiRequest_GetErrorScreen(client, false, false);
	}

}

void ApiRequest_GetIndividualFile(EthernetClient *client, char* filename, char* arguments)
{
	/* in PUT request pass is passed as: 
	01234567
	A=: 1234
	*/
	if (isPassCorrect(&arguments[4]))
	{
		// this time no space after the /, so a sub-file!
		//char *filename;

		//Next 2 lines create pointer of sub string for filename and trims clientline
		//filename = clientline + 10; // look after the "GET /file/" (10 chars)
		// a little trick, look for the " HTTP/1.1" string and 
		// turn the first character of the substring into a 0 to clear it out.
		(strstr(filename, " HTTP"))[0] = 0;

		if (file.open(&root, filename, O_READ)) {
			ApiRequest_GetSuccessHeader(client, filename);
			int16_t c;
			while ((c = file.read()) > 0) {
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

void ApiRequest_GetIndividualFile(EthernetClient *client, char* filename)
{
	    // turning 'cache.appcache' into 'cache.app' (sd supports only 8.3 file structure)
		if (strstr(filename, "cache HTTP") != 0){
			(strstr(filename, "cache HTTP"))[0] = 0;
		}
		else{
			(strstr(filename, " HTTP"))[0] = 0;
		}

		if (file.open(&root, filename, O_READ)) {
			ApiRequest_GetSuccessHeader(client, filename);
			int16_t c;
			while ((c = file.read()) > 0) {
				(*client).print((char)c);
			}
			file.close();
		}
		else{
			ApiRequest_GetErrorScreen(client, true, false);
		}
}

void ApiRequest_GetRam(EthernetClient *client)
{
	ApiRequest_GetSuccessHeader(client, ".JSN");
	(*client).print(F("{ \"min\":\""));
	(*client).print(minMaxRam[0], DEC);
	(*client).print(F("\", \"max\":\""));
	(*client).print(minMaxRam[1], DEC);
	(*client).print(F("\" }"));
}

void ApiRequest_GetDateTime(EthernetClient *client)
{
	ApiRequest_GetSuccessHeader(client, ".JSN");
	(*client).print(F("{ \"datetime\":\""));
	printCurrentStringDate(client);
	(*client).print(F("\" }"));

	/*printCurrentStringDate_Debug(client);*/
}

void ApiRequest_PutSettings(EthernetClient *client, char* arguments)
{
	//012345678901234567 
	//A=55555&U=55555111
	if (isPassCorrect(&arguments[2]))
	{
		settings[0] = arguments[10];
		settings[1] = arguments[11];
		settings[2] = arguments[12];
		settings[3] = arguments[13];
		settings[4] = arguments[14]; //admin pass
		settings[5] = arguments[15]; //log internet traffic
		settings[6] = arguments[16]; //log frequency in minutes
		settings[7] = arguments[17]; //keep log of free ram per session
		saveSettings();

		ApiRequest_GetSuccessHeader(client, ".JSN");
		(*client).println(F("{ \"response\":\"Success\" }"));
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

void ApiRequest_PutDateTime(EthernetClient *client, char* arguments)
{
	//          10        20       30         40
	//01234567890123456789012345678901234567890 
	//A=55555&D=YYMMDDhhmmssw
	//A=55555&D=YYMMDDhhmmssw

	//to update clock do:
	//setDS3231time(15, 2, 10, 23, 43, 10, 3);  //2015-05-22 21:15:30 tuesday
	if (isPassCorrect(&arguments[2]))
	{
		/* should we check here if all numbers are between 0 amd 9, otherwise error out? */
		setDS3231time(
			toDec(arguments[10], arguments[11]), //y
			toDec(arguments[12], arguments[13]), //m
			toDec(arguments[14], arguments[15]), //d
			toDec(arguments[16], arguments[17]), //h
			toDec(arguments[18], arguments[19]), //m
			toDec(arguments[20], arguments[21]), //s
			toDec(arguments[22])                 //w			
			);
		ApiRequest_GetSuccessHeader(client, ".JSN");
		(*client).print(F("{ \"response\":\"Success\", \"newdate\" : \""));
		printCurrentStringDate(client);
		(*client).println(F("\" }"));
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

byte toDec(char A, char B)
{
	logRamUsage();
	/* convert each ASCII to byte value and add them 
	Example:
	VAR  A     ,B
	ASCI 1      5
	DEC: 49    ,53    
	BIN: 110001,110101	
	*/
	return ((A - '0') * 10 + (B - '0'));
}
byte toDec(char A)
{
	return A - '0';
}

void ApiRequest_PutReboot(EthernetClient *client, char* arguments)
{
	if (isPassCorrect(&arguments[2]))
	{		
		ApiRequest_GetSuccessHeader(client, ".JSN");
		(*client).println(F("{ \"response\":\"Success\" }"));
		resetFunc();
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}

}

/*############################ List FIles ########################################*/

void ListFiles(EthernetClient *client, uint8_t flags) {
	// This code is just copied from SdFile.cpp in the SDFat library
	// and tweaked to print to the client output in html!
	dir_t p;

	root.rewind();
	(*client).println(F("<ul>"));
	while (root.readDir(p) > 0) {
		// done if past last used entry
		if (p.name[0] == DIR_NAME_FREE) break;

		// skip deleted entry and entries for . and  ..
		if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

		// only list subdirectories and files
		if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

		// print any indent spaces
		(*client).print(F("<li><a href=\"file/"));
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				(*client).print(F("."));
			}
			(*client).print((char)p.name[i]);
		}
		(*client).print(F("\">"));

		// print file name with possible blank fill
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				(*client).print(F("."));
			}
			(*client).print((char)p.name[i]);
		}

		(*client).print(F("</a>"));

		if (DIR_IS_SUBDIR(&p)) {
			(*client).print(F("/"));
		}

		// print modify date/time if requested
		if (flags & LS_DATE) {
			(*client).print(FAT_YEAR(p.lastWriteDate));
			(*client).print(F("-"));
			printTwoDigits(client, FAT_MONTH(p.lastWriteDate));
			(*client).print(F("-"));
			printTwoDigits(client, FAT_DAY(p.lastWriteDate));
			(*client).print(F(" "));
			printTwoDigits(client, FAT_HOUR(p.lastWriteTime));
			(*client).print(F(":"));
			printTwoDigits(client, FAT_MINUTE(p.lastWriteTime));
			(*client).print(F(":"));
			printTwoDigits(client, FAT_SECOND(p.lastWriteTime));
			logRamUsage();
		}
		// print size if requested
		if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
			(*client).print(F(" "));
			(*client).print(p.fileSize);
		}
		(*client).println(F("</li>"));
	}
	(*client).println(F("</ul>"));
}

void printTwoDigits(EthernetClient *client, uint8_t v) {

	(*client).print('0' + v / 10);
	(*client).print('0' + v % 10);
	(*client).print(0);
}

/*############################ Clock #########################################*/

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
void printCurrentStringDateToFile(SdFile *file, bool getCurrentTime)
{
	if (getCurrentTime){
		//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
		// retrieve data from DS3231
		readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
		// send it to the serial monitor
	}
	else{
		year = session[0];
		month = session[1];
		dayOfMonth = session[2];
		hour = session[3];
		minute = session[4];
		second = session[5];
		dayOfWeek = session[6];
	}

	(*file).print(F("20"));
	(*file).print(itoa(year, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (month < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(month, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (dayOfMonth < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(dayOfMonth, dateTimeConversionValue, DEC));
	(*file).print(F(" "));
	if (hour < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(hour, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (minute < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(minute, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (second < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(second, dateTimeConversionValue, DEC));

	//	file.print(day[dayOfWeek]);

}

void printCurrentStringDate(EthernetClient *file)
{

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor

	(*file).print(F("20"));
	(*file).print(itoa(year, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (month < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(month, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (dayOfMonth < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(dayOfMonth, dateTimeConversionValue, DEC));
	(*file).print(F(" "));
	if (hour < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(hour, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (minute < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(minute, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (second < 10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(second, dateTimeConversionValue, DEC));
	(*file).print(F(" "));
	(*file).print(day[dayOfWeek]);

}

//void printCurrentStringDate_Debug(EthernetClient *file)
//{
//
//	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
//	// retrieve data from DS3231
//	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
//	// send it to the serial monitor
//	
//	(*file).println();
//	(*file).print(year, DEC);
//	(*file).print(" ");
//	(*file).print(month, DEC);
//	(*file).print(" ");
//	(*file).print(dayOfMonth, DEC);
//	(*file).print(" ");
//	(*file).print(hour, DEC);
//	(*file).print(" ");
//	(*file).print(minute, DEC);
//	(*file).print(" ");
//	(*file).print(second, DEC);
//	(*file).print(" ");
//	(*file).print(dayOfWeek, DEC);
//
//
//	(*file).println();
//	(*file).print(year, BIN);
//	(*file).print(" ");
//	(*file).print(month, BIN);
//	(*file).print(" ");
//	(*file).print(dayOfMonth, BIN);
//	(*file).print(" ");
//	(*file).print(hour, BIN);
//	(*file).print(" ");
//	(*file).print(minute, BIN);
//	(*file).print(" ");
//	(*file).print(second, BIN);
//	(*file).print(" ");
//	(*file).print(dayOfWeek, BIN);
//	(*file).print(" ");
//	(*file).println();
//
//
//	(*file).print(year, HEX);
//	(*file).print(" ");
//	(*file).print(month, HEX);
//	(*file).print(" ");
//	(*file).print(dayOfMonth, HEX);
//	(*file).print(" ");
//	(*file).print(hour, HEX);
//	(*file).print(" ");
//	(*file).print(minute, HEX);
//	(*file).print(" ");
//	(*file).print(second, HEX);
//	(*file).print(" ");
//	(*file).print(dayOfWeek, HEX);
//
//	(*file).write(year);
//	(*file).print(" ");
//	(*file).write(month);
//	(*file).print(" ");
//	(*file).write(dayOfMonth);
//	(*file).print(" ");
//	(*file).write(hour);
//	(*file).print(" ");
//	(*file).write(minute);
//	(*file).print(" ");
//	(*file).write(second);
//	(*file).print(" ");
//	(*file).write(dayOfWeek);
//}

char* getCurrentLogFileName(){

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor


	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[1]))); //'data'
	strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	if (month < 10)
	{
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[0]))); //'0'
	}
	strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[3]))); //'.jsn'

	return logFileName;
}

char* getCurrentErrorFileName(){

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor

	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[2]))); //'err_'
	strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	if (month < 10)
	{
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[0]))); //'0'
	}
	strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[4]))); //'.err'

	return logFileName;
}
/****************************logging***************************************/


void logInit(){
	//Serial.begin(9600);
}

void error(char* line){


	//Serial.println(line);
	//strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
	//if (file.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
	//	if (file.isFile()){
	//		file.print(F("{ \"Error\":\""));
	//		file.print(line);
	//		file.print(F("\", "));
	//		file.print(F("\"datetime\":\""));
	//		file.print(getCurrentErrorFileName());
	//		file.print(F("\" }"));
	//		file.println();
	//	}
	//	file.close();
	//}
	//else{
	//	cryticalError();
	//}
}

void error(const char* line){
	//Serial.print(F("Error:"));
	//Serial.println(line);

	//SdFile file;
	//file.open(root, getCurrentErrorFileName(), O_CREAT | O_APPEND | O_WRITE);    //Open or create the file
	//if (file.isFile()){

	//	file.print(F("{ \"date\": "));
	//	file.print(getCurrentStringDate(false));
	//	file.print(F("\", \"Error\": "));
	//	file.print(line);
	//	file.println(F("\" }"));
	//}
}

void error(const __FlashStringHelper* line)
{
	//Serial.print(F("Error:"));
	//Serial.println(line);

	//SdFile file;
	//file.open(root, getCurrentErrorFileName(), O_CREAT | O_APPEND | O_WRITE);    //Open or create the file
	//if (file.isFile()){

	//	file.print(F("{ \"date\": "));
	//	file.print(getCurrentStringDate(false));
	//	file.print(F("\", \"Error\": "));
	//	file.print(line);
	//	file.println(F("\" }"));
	//}

}

void logHttp(char* line, char* args){

	//Serial.println(line);
	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
	if (file.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
		if (file.isFile()){
			file.print(F("{ \"HTTP\":\""));
			file.print(line);
			file.print(F("\", \"params\":\""));
			file.print(args);
			file.print(F("\", "));
			file.print(F("\"datetime\":\""));
			printCurrentStringDateToFile(&file, true);
			file.print(F("\" }"));
			file.println();
		}
		file.close();
	}
}

void log(char* line){

	//Serial.println(line);
	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[6]))); //'log.txt'
	if (file.open(&root, logFileName, O_APPEND | O_WRITE | O_CREAT)) {
		if (file.isFile()){
			file.print(F("{ \"debug\":\""));
			file.print(line);
			file.print(F("\", "));
			file.print(F("\"datetime\":\""));
			printCurrentStringDateToFile(&file, true);
			file.print(F("\" }"));
			file.println();
		}
		file.close();
	}
}

void cryticalError(){


	resetFunc();  //call reset
}

//int freeRam()
//{
//	extern int __heap_start, *__brkval;
//	int v;
//	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
//}
/**************************************************************************/