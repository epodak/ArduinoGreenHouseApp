
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



#define BUFSIZ 100
#define TIMEOUTMS 2000
#define LOGLINELENGTH 100
#define SENSORVALUELENGTH 10

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
char logLineLength[LOGLINELENGTH];
char* sensorValue;

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

	pinMode(outputEthernetPin, OUTPUT);                       // set the SS pin as an output (necessary!)
	digitalWrite(outputEthernetPin, HIGH);                    // but turn off the W5100 chip!

	SdInit();
	EthernetInit();
}


void loop()
{
	checkForApiRequests();
	SDWriteLogPins("2015-05-22 13:33:18");

	delay(3000);

}


void SdInit(){
	if (!card.init(SDISPEED, outputSdPin)) error(F("card.init failed!"));
	if (!volume.init(&card)) error(F("vol.init failed!"));
	if (!root.openRoot(&volume)) error(F("openRoot failed"));
}

void SDWriteLogPins(const char* DateTimeStamp)
{
	const int numOfPins = 6;
	//// yyMMddhhmm+127,+127,+127,+127,+127,+127
	//// 1234567890    12345

	strcpy(logLineLength,DateTimeStamp);
	strcat(logLineLength, ":");

	for (uint8_t analogPin = 0; analogPin < numOfPins; analogPin++) {
		int sensor = analogRead(analogPin);
		//dataString += String(sensor);
		sensorValue = itoa(sensor, sensorValue, 10);
		strcat(logLineLength, sensorValue);
		if (analogPin < (numOfPins - 1)) {
			strcat(logLineLength, ",");
		}
	}

	SdFile file;
	file.open(root, "datalog.txt", O_CREAT | O_APPEND | O_WRITE);    //Open or create the file
	if (file.isFile()){

		file.println(logLineLength);
		file.close();
		log(logLineLength);
	}
	else{
		error(F("error opening datalog.txt"));
	}
}

void EthernetInit(){
	Ethernet.begin(mac, ip);
	server.begin();
}

void checkForApiRequests()
{
	char clientline[BUFSIZ];
	int index = 0;

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
					client.println(F("<li><a href=\"#\">current sensor values</a></li>"));
					client.println(F("</ul></html>"));
				}
				/******************************check for files*********************************/
				else if (strstr(clientline, "GET /file/") != 0) {
					// this time no space after the /, so a sub-file!
					char *filename;

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
					client.println(F("Content-Type: text/plain"));
					client.println(F("Connection: close"));
					client.println();

					int16_t c;
					while ((c = file.read()) > 0) {
						client.print((char)c);
					}
					file.close();
				}
				/*******************************************************************************/
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
					ListFiles(client, LS_SIZE ); //LS_SIZE | LS_DATE
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

void ListFiles(EthernetClient client, uint8_t flags) {
	// This code is just copied from SdFile.cpp in the SDFat library
	// and tweaked to print to the client output in html!
	dir_t p;

	root.rewind();
	client.println("<ul>");
	while (root.readDir(p) > 0) {
		// done if past last used entry
		if (p.name[0] == DIR_NAME_FREE) break;

		// skip deleted entry and entries for . and  ..
		if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

		// only list subdirectories and files
		if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

		// print any indent spaces
		client.print("<li><a href=\"file/");
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				client.print('.');
			}
			client.print((char)p.name[i]);
		}
		client.print("\">");

		// print file name with possible blank fill
		for (uint8_t i = 0; i < 11; i++) {
			if (p.name[i] == ' ') continue;
			if (i == 8) {
				client.print('.');
			}
			client.print((char)p.name[i]);
		}

		client.print("</a>");

		if (DIR_IS_SUBDIR(&p)) {
			client.print('/');
		}

		// print modify date/time if requested
		if (flags & LS_DATE) {
			client.print(FAT_YEAR(p.lastWriteDate));
			client.print('-');
			printTwoDigits(client, FAT_MONTH(p.lastWriteDate));
			client.print('-');
			printTwoDigits(client, FAT_DAY(p.lastWriteDate));
			client.print(' ');
			printTwoDigits(client, FAT_HOUR(p.lastWriteTime));
			Serial.print(':');
			printTwoDigits(client, FAT_MINUTE(p.lastWriteTime));
			Serial.print(':');
			printTwoDigits(client, FAT_SECOND(p.lastWriteTime));

		}
		// print size if requested
		if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
			client.print(' ');
			client.print(p.fileSize);
		}
		client.println("</li>");
	}
	client.println("</ul>");
}

void printTwoDigits(EthernetClient client, uint8_t v) {
	char str[3];
	str[0] = '0' + v / 10;
	str[1] = '0' + v % 10;
	str[2] = 0;
	client.print(str);
}

void logInit(){
	Serial.begin(9600);
}

void error(char* line){
	//Serial.print(F("Error:"));
	//Serial.println(line);
}

void error(const char* line){
	//Serial.print(F("Error:"));
	//Serial.println(line);
}

void error(const __FlashStringHelper*)
{

}

void log(char* line){

	//Serial.println(line);
}