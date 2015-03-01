
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
//#include <avr/eeprom.h>   //slow (3ms) [therefore use it only for startup variables], 100,000x 

/* ################# Hardware Settings ############################ */
/* address for clock on I2C bus */
#define DS3231_I2C_ADDRESS 0x68
#define outputEthernetPin 10
#define outputSdPin 4
//#define outputLcdPin 8
#define SDISPEED SPI_HALF_SPEED
//  SPI_FULL_SPEED

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
const prog_char string_8[] PROGMEM = "20";
const prog_char string_9[] PROGMEM = "-";
const prog_char string_10[] PROGMEM = ":";
const prog_char string_11[] PROGMEM = " ";

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
	string_11
};

/************ DYNAMIC VARS ************/
/*
datayyMM.jsn //has to be 8.3 format or it will error out! */
#define LOGFILENAMELENGTH 13 
char logFileName[LOGFILENAMELENGTH];
char* sensorValue;
char* dateTimeConversionValue;
//const char* day[] = { F("NotSet!"), F("Sun"), F("Mon"), F("Tue"), F("Wed"), F("Thu"), F("Fri"), F("Sat") };

byte lastLogTimeMin; //save minute of the last sensor log
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
=> 4 chars + str end = 5 chars  */
#define SENSORVALUELENGTH 5
/*2015 or 05 or 22:
=> 4 chars + str end = 5 chars */
#define DATETIMECONVERSIONLENGTH 5
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

//#define SERIALDEBUG

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
	if (!card.init(SDISPEED, outputSdPin)) cryticalError(1);
	if (!volume.init(&card)) cryticalError(1);
	if (!root.openRoot(&volume)) cryticalError(1);
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
	if (settings[7] == '0') return;
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
			file.print(F("{ \"Session\":\""));
			file.print(sessionId[0], DEC);
			file.print(sessionId[1], DEC);
			file.print(F("\", \"FreeRam\":\" { \"From\" :\""));

			file.print(minMaxRam[0], DEC);
			file.print(F("\", \"To\" :\""));
			file.print(minMaxRam[1], DEC);
			file.print(F("\" }, \"StartedTime\":\""));

			printCurrentStringDateToFile(&file, false); //print session start time from session ID
			file.print(F("\", \"EndedTime\":\""));
			printCurrentStringDateToFile(&file, true); //print current time
			file.print(F("\" }                            ")); //space to delete trailing chars from previous session

		}
		file.close();
	}
	else{
		cryticalError(1);
	}
}

/* log timer allows writting every X mins */
bool isTimeToLog()
{
	byte min;

	if (settings[6] == '0'){ min = 10; }     //log lines: 6/h, 144/d, 4320/month
	else if (settings[6] == '1'){ min = 30; }//log lines: 2/h, 48/d,  1488/month
	//else if (settings[6] == '2'){ min = 60; }//log lines: 1/h, 24/d,  744/month
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
		settings[6] = '2'; //log frequency in minutes ( 0=10min, 1=30min, 2=60min)
		settings[7] = '1'; //keep log of free ram per session
		settings[8] = '0'; //Reboot if can not write to SD
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
		file.print(F("                      "));
		file.close();
	}
	else{
		cryticalError(1);
	}

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

			/*file.print(F(" \"Id\":\""));
			file.print(sessionId[0], DEC);
			file.print(sessionId[1], DEC);
			file.print(F("\", "));*/

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
		cryticalError(1);
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
				if (strstr(request, "GET / ") != 0) {
					ApiRequest_GetFile(&client, "index.gz", NULL);
				}
				/****************************** manifest file **********************************/
				else if (strstr(request, "GET /cache.app") != 0) {
					//ApiRequest_GetIndividualFile(&client, request + 5);
					ApiRequest_GetFile(&client, request + 5, NULL);
					//ApiRequest_GetErrorScreen(&client, true, false);
				}
				/****************************** login  (ANYONE) ********************************/
				else if (strstr(request, "GET /admin?U=") != 0) {
					ApiRequest_CheckLogInPin(&client, request + 13);
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
					//?U=
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
	/* instead try read one line at the time and parse, "GET/PUT/POST/OPTION", "PP:" and  "Accept-Encoding: gzip, deflate" lines 
				strstr(request, "GET / ") != 0
				strstr(request, "PP: ") != 0
				strstr(request, "Accept-Encoding: ") != 0  && strstr(request, "gzip") != 0 
	*/

	uint8_t index = 0;
	uint8_t indexargs = 0;
	bool parametersFound = false;
	int c = (*client).read();
	//putheader[0] = c;
	while (c >= 0){
		// If it isn't a new line, add the character to the buffer
		if (c != '\n' && c != '\r') {
			/*---- find request ----------------------*/
			if (index  < BUFSIZ)
			{
				request[index] = c; //read first line
				index++;
			}
			/*---- find post or put arguments --------*/
			if (!parametersFound){ //look for PP:
				putheader[indexargs++] = c; //save 0,1,2 for comparison
				if (indexargs == 4) {
					putheader[0] = putheader[1];
					putheader[1] = putheader[2];
					putheader[2] = putheader[3];
					indexargs--; //indexargs is 3 again

					if (putheader[0] == 'P' && putheader[1] == 'P' && putheader[2] == ':'){
						parametersFound = true;
					}
				}	
			}
			else{
				if (indexargs < BUFSIZ){					
					putheader[indexargs] = c;
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
				putheader[indexargs] = 0;
				indexargs = BUFSIZ + 1;
			}		
		}
		logRamUsage();
		c = (*client).read();
	}//end while
	//folowing just prevents any bug if request line is very long 
	//so we did not have chance to break the line yet:
	putheader[BUFSIZ - 1] = 0;
	request[BUFSIZ - 1] = 0;
#ifdef SERIALDEBUG
	Serial.print("request:");
	Serial.println(request);
	Serial.print("putheader:");
	Serial.println(putheader);
#endif
}

void ApiRequest_GetSuccessHeader(EthernetClient *client, char* filename)
{
	(*client).println(F("HTTP/1.1 200 OK"));
	if (strstr(filename, ".JSN") != 0)
		(*client).println(F("Content-Type: application/json"));
	else if (strstr(filename, ".CHE") != 0)
		(*client).println(F("Content-Type: text/cache-manifest"));
	else if (strstr(filename, ".GZ") != 0){
		(*client).println(F("Content-Encoding: gzip"));
		(*client).println(F("Content-Type: text/html"));
	}
	else
		(*client).println(F("Content-Type: text/html"));	
	

	if (true){ //if settings allow other javascript websites to query our api
		(*client).println(F("Access-Control-Allow-Origin: *"));
		(*client).println(F("Access-Control-Allow-Headers: PP"));
	}
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

	if (true){ //if settings allow other javascript websites to query our api
		(*client).println(F("Access-Control-Allow-Origin: *"));
		(*client).println(F("Access-Control-Allow-Headers: PP"));
	}
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

void ApiRequest_GetOptionsScreen(EthernetClient *client)
{

	(*client).println(F("HTTP/1.1 200 OK"));
	if (true){ //if settings allow other javascript websites to query our api
		(*client).println(F("Allow: GET, PUT, POST, OPTIONS"));
		(*client).println(F("Access-Control-Allow-Origin: *")); //add settings to allow specific origins too?
		(*client).println(F("Access-Control-Allow-Headers: PP"));
		(*client).println(F("Access-Control-Allow-Methods: GET, PUT, POST, OPTIONS"));
	}
	(*client).println(F("Connection: close"));
	(*client).println();
}

void ApiRequest_CheckLogInPin(EthernetClient *client, char* pass){
	
	(strstr(pass, " HTTP"))[0] = 0;
	if (isPassCorrect(pass))
	{
		ApiRequest_GetSuccessHeader(client, ".JSN");
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}
}

void ApiRequest_GetFileList(EthernetClient *client, char* putheader){

	/* in PUT request pass is passed as header:*/
	if (isPassCorrect(putheader + 4))
	{
		ApiRequest_GetSuccessHeader(client, ".JSN");
		ListFiles(client, LS_SIZE); //LS_SIZE 
	}
	else{
		ApiRequest_GetErrorScreen(client, false, true);
	}

}

void ApiRequest_GetFile(EthernetClient *client, char* filename, char* putheader)
{
	/* in PUT request pass is passed as header */
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
		// a little trick, look for the " HTTP/1.1" string and 
		// turn the first character of the substring into a 0 to clear it out.
		(strstr(filename, " HTTP"))[0] = 0;

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
		strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[7]))); //'settings.txt'
		if (file.open(&root, logFileName, O_READ)) {
			ApiRequest_GetSuccessHeader(client, ".JSON");

			(*client).print(F("{ \"DeviceTime\":\""));
			printCurrentStringDate(client);							
			(*client).print(F("\", \"MinRam\" : \""));
			(*client).print(minMaxRam[0], DEC);
			(*client).print(F("b\", \"MaxRam\" : \""));
			(*client).print(minMaxRam[1], DEC);
			(*client).print(F("b\", \"RunningSince\" : \""));
			printCurrentStringDateToClient(client, false);
			(*client).print(F("\", \"Settings\" : \""));
			int16_t c;
			while ((c = file.read()) >= 0) {
				if (c != 32) //trim spaces
					(*client).print((char)c);
			}
			file.close();
			(*client).print(F("\" }"));
		}
		else{			
			ApiRequest_GetErrorScreen(client, true, true);
		}
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

		ApiRequest_GetSuccessHeader(client, ".JSN");
		(*client).println(F("{ \"response\":\"Success\" }"));
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
	return (toDec(A) * 10 + (B - '0'));
}
byte toDec(char A)
{
	return A - '0';
}

void ApiRequest_PutReboot(EthernetClient *client, char* arguments)
{
	if (isPassCorrect(arguments + 4))
	{		
		ApiRequest_GetSuccessHeader(client, ".JSN");
		(*client).println(F("{ \"response\":\"Success\" }"));
		delay(1);
		(*client).stop();
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
	bool first = true;
	root.rewind();
	(*client).print(F("{ ["));
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
		(*client).print(F(" { \""));
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				(*client).print(F("."));
			}
			(*client).print((char)p.name[i]);
		}
		(*client).print(F("\" : "));

		// print size if requested
		if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
			(*client).print(p.fileSize);
		}
		(*client).println(F(" }"));
	}
	(*client).print(F("] }"));

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
void printCurrentStringDateToClient(EthernetClient *file, bool getCurrentTime)
{
	if (getCurrentTime){
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
#ifdef SERIALDEBUG
	Serial.begin(9600);
#endif
}

void serialShow(char* line){
#ifdef SERIALDEBUG
	Serial.println(line);
#endif
}

//void serialShow(byte b){
//#ifdef SERIALDEBUG
//	Serial.println(b);
//#endif
//}
//
//void error(char* line){
//
//
//}
//
//void error(const char* line){
//
//}
//
//void error(const __FlashStringHelper* line)
//{
//
//
//}

void logHttp(char* line, char* args){

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

/**************************************************************************/