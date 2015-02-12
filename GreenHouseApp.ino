
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
#include <avr/pgmspace.h>

/* ################# Hardware Settings ############################ */
/* address for clock on I2C bus */
#define DS3231_I2C_ADDRESS 0x68
#define outputEthernetPin 10
#define outputSdPin 4
#define outputLcdPin 8
#define SDISPEED SPI_HALF_SPEED

/************ ETHERNET STUFF ************/
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 2, 120 };
EthernetServer server(80);

/************ SDCARD STUFF ************/
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

/************ TIMER STUFF *************/
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

/************ PROGMEM STUFF *************/
prog_char string_0[] PROGMEM = "0";   // "String 0" etc are strings to store - change to suit.
prog_char string_1[] PROGMEM = "data";
prog_char string_2[] PROGMEM = "err_";
prog_char string_3[] PROGMEM = ".jsn";
prog_char string_4[] PROGMEM = ".err";

PROGMEM const char *string_table[] = 	   // change "string_table" name to suit
{
	string_0,
	string_1,
	string_2,
	string_3,
	string_4 };

/************ DYNAMIC VARS ************/
/*
datayyMM.jsn //has to be 8.3 format or it will error out! */
#define LOGFILENAMELENGTH 13 
char logFileName[LOGFILENAMELENGTH];
char* sensorValue;
char* dateTimeConversionValue;
const char* day[] = { "NotSet!", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

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
PUT /date?y=15&M=2&d=15&h=23&m=55&s=15&w=2  */
#define BUFSIZ 60
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

	//to update clock do:
	//setDS3231time(15, 2, 10, 
	//	          23, 43, 10, 
	//			  3);  //2015-05-22 21:15:30 friday

	SdInit();
	EthernetInit();
}


void loop()
{
	checkForApiRequests(); //check any API calls and respond 
	LogSensors();      //log sensor input to log file

	delay(3000);

}


void SdInit(){
	if (!card.init(SDISPEED, outputSdPin)) error(F("card.init failed!"));
	if (!volume.init(&card)) error(F("vol.init failed!"));
	if (!root.openRoot(&volume)) error(F("openRoot failed"));
}


void EthernetInit(){
	Ethernet.begin(mac, ip);
	server.begin();
}

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

	/******************/
	//check for right time to log sensors!
	//otherwise skip
	//return;

	//AnalogPins: 0,1,2,3 (4 & 5 are reserved for I2C so no need to log it)
	const uint8_t numOfPins = 4; 
	int sensor = 0;  //int8_t not right


	//SdFile file; /* use global?? */
	file.open(root, getCurrentLogFileName(), O_CREAT | O_APPEND | O_WRITE);    //Open or create the file
	if (file.isFile()){

		file.print(F("{ \"datetime\":\""));
		printCurrentStringDate(&file);
		file.print(F("\", "));

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
		}
		file.print(F("}"));	
		file.println();

		file.close();		
	}
	else{
		error(F("error opening log file"));
	}
}

/* 
char clientline[BUFSIZ];
uint8_t index
EthernetClient client
*/

void checkForApiRequests()
{
	char clientline[BUFSIZ];
	uint8_t index = 0;

	EthernetClient client = server.available();
	if (client) {
		index = 0;		// reset the input buffer

		while (client.connected()) {
			if (client.available()) {
				char c = client.read();

				// If it isn't a new line, add the character to the buffer
				if (c != '\n' && c != '\r') {
					clientline[index] = c;
					index++;
					// are we too big for the buffer? start tossing out data
					if (index >= BUFSIZ) index = BUFSIZ - 1;
					// continue to read more data!
					continue; //restart while
				}

				// got a \n or \r new line, which means the string is done
				clientline[index] = 0;

				// Print it out for debugging
				log(clientline);

				// Look for substring such as a request to get the root file
				/*****************************home page*****************************************/
				if (strstr(clientline, "GET / ") != 0) {
					// send a standard http response header
					client.println(F("HTTP/1.1 200 OK"));
					client.println(F("Content-Type: text/html"));
					client.println(F("Connection: close"));
					client.println();
					client.println(F("<html><h2>Welcome</h2>"));
					client.println(F("<ul>"));
					client.println(F("<li><a href=\"files\">view list of files</a></li>"));
					client.println(F("<li><a href=\"sensors\">current sensor values</a></li>"));
					client.println(F("<li><a href=\"clock\">view and update app clock</a></li>"));
					client.println(F("</ul></html>"));
				}
				/****************************** file content *********************************/
				else if (strstr(clientline, "GET /file/") != 0) {
					// this time no space after the /, so a sub-file!
					char *filename;

					//Next 2 lines create pointer of sub string for filename and trims clientline
					filename = clientline + 10; // look after the "GET /file/" (10 chars)
					// a little trick, look for the " HTTP/1.1" string and 
					// turn the first character of the substring into a 0 to clear it out.
					(strstr(clientline, " HTTP"))[0] = 0;

					// print the file we want
					log(filename);

					if (!file.open(&root, filename, O_READ)) {
						client.println(F("HTTP/1.1 404 Not Found"));
						client.println(F("Content-Type: text/html"));
						client.println(F("Connection: close"));
						client.println();
						client.println(F("<h2>File Not Found!</h2>"));
						break;
					}

					client.println(F("HTTP/1.1 200 OK"));
					//dynamically asign content type by file extension?
					if (strstr(filename, ".JSN") != 0){
						client.println(F("Content-Type: application/json"));
					}
					else if (strstr(filename, ".HTM") != 0){
						client.println(F("Content-Type: text/html; charset=utf-8"));
					}
					else{
						client.println(F("Content-Type: text/plain"));
					}					
					client.println(F("Connection: close"));
					client.println();

					int16_t c;
					while ((c = file.read()) > 0) {
						client.print((char)c);
					}
					file.close();
				}
				/***************************** list files *************************************/
				else if (strstr(clientline, "GET /files") != 0) {
					// send a standard http response header
					client.println(F("HTTP/1.1 200 OK"));
					client.println(F("Content-Type: text/html"));
					client.println(F("Connection: close"));
					client.println();
					client.print(F("<p>Free RAM:"));
					client.println(FreeRam());
					client.print(F(" bytes</p>"));
					// print all the files, use a helper to keep it clean
					client.println(F("<h2>Files:</h2>"));
					ListFiles(&client, LS_SIZE ); //LS_SIZE | LS_DATE
				}
				/***************************** show clock *************************************/
				else if (strstr(clientline, "GET /clock") != 0) {
					// send a standard http response header
					client.println(F("HTTP/1.1 200 OK"));
					client.println(F("Content-Type: text/html"));
					client.println(F("Connection: close"));
					client.println();
					client.print(F("<p>Time on device is:"));
					printCurrentStringDate(&client);
					client.print(F(" </p>"));
					// print all the files, use a helper to keep it clean					
				}
				/**********************************404******************************************/
				else {
					// everything else is a 404
					client.println(F("HTTP/1.1 404 Not Found"));
					client.println(F("Content-Type: text/html"));
					client.println(F("Connection: close"));
					client.println();
					client.println(F("<h2>File Not Found!</h2>"));
				}
				break;
			}
		}
		// give the web browser time to receive the data		
		delay(1);
		client.stop();
	}

}

/****************************List FIles***************************************/

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

/****************************Clock***************************************/
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
void setDS3231time (byte  year, byte  month, byte  dayOfMonth, byte  hour, byte  minute, byte  second, byte  dayOfWeek )
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
void readDS3231time(byte *year, byte *month, byte *dayOfMonth, byte *hour, byte *minute, byte *second, byte *dayOfWeek )
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
void printCurrentStringDate(SdFile *file)
{

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time( &year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor

	(*file).print(F("20"));
	(*file).print(itoa(year, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (month<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(month, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (dayOfMonth<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(dayOfMonth, dateTimeConversionValue, DEC));
	(*file).print(F(" "));
	if (hour<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(hour, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (minute<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(minute, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (second<10)
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
	if (month<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(month, dateTimeConversionValue, DEC));
	(*file).print(F("-"));
	if (dayOfMonth<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(dayOfMonth, dateTimeConversionValue, DEC));
	(*file).print(F(" "));
	if (hour<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(hour, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (minute<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(minute, dateTimeConversionValue, DEC));
	(*file).print(F(":"));
	if (second<10)
	{
		(*file).print(F("0"));
	}
	(*file).print(itoa(second, dateTimeConversionValue, DEC));

	//	file.print(day[dayOfWeek]);

}

char* getCurrentLogFileName(){

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor


	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[1]))); //'data'
	strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	if (month<10)
	{
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[0]))); //'0'
	}
	strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[3]))); //'.jsn'

	/*strcpy(logFileName, "data");
	strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	if (month<10)
	{
		strcat(logFileName, "0");
	}
	strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	strcat(logFileName, ".jsn");*/
	return logFileName;
}

char* getCurrentErrorFileName(){

	//byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
	// retrieve data from DS3231
	readDS3231time(&year, &month, &dayOfMonth, &hour, &minute, &second, &dayOfWeek);
	// send it to the serial monitor

	strcpy_P(logFileName, (char*)pgm_read_word(&(string_table[2]))); //'err_'
	strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	if (month<10)
	{
		strcat_P(logFileName, (char*)pgm_read_word(&(string_table[0]))); //'0'
	}
	strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	strcat_P(logFileName, (char*)pgm_read_word(&(string_table[4]))); //'.err'

	//strcpy(logFileName, "err_");
	//strcat(logFileName, itoa(year, dateTimeConversionValue, DEC));
	//if (month<10)
	//{
	//	strcat(logFileName, "0");
	//}
	//strcat(logFileName, itoa(month, dateTimeConversionValue, DEC));
	//strcat(logFileName, ".err");
	return logFileName;
}
/****************************logging***************************************/


void logInit(){
	//Serial.begin(9600);
}

void error(char* line){
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
	//	file.close();
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

void log(char* line){

	//Serial.println(line);
}
/**************************************************************************/