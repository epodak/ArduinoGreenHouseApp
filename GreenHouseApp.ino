
//ethernet SD chip uses digital pins 10, 11, 12, and 13 for SPI buss.


#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>




class SDCard
{
public:

	SdFile file;

	void initRoot(int chipSelected)
	{
		chipSelect = chipSelected;
		if (!card.init(SPI_HALF_SPEED, chipSelect)){
			Serialprintln("initialization failed.");
			return;
		}

		if (!volume.init(card)) {
 
			Serialprintln("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
			return;
		}

		root.openRoot(volume);

	}

	void SDWriteLogPins(String DateTimeStamp)
	{
		root.openRoot(volume);
		//initRoot(chipSelect);

		int numberOfPins = 6; //number of analog pins to log
		String dataString = DateTimeStamp + ":";
		for (int analogPin = 0; analogPin < numberOfPins; analogPin++) {
			int sensor = analogRead(analogPin);
			dataString += String(sensor);
			if (analogPin < (numberOfPins - 1)) {
				dataString += ",";
			}
		}

		SdFile file;
		file.open(root, "datalog.txt", O_CREAT | O_APPEND | O_WRITE);    //Open or create the file 'name' in 'root' for writing to the end of the file.
		if (file.isFile()){

			file.println(dataString);
			file.close();
			// print to the serial port too:
			Serialprintln(dataString);
		}
		else{
			Serialprintln("error opening datalog.txt");
		}
	}

	void SDGetCardInfo()
	{
		root.openRoot(volume);
		//initRoot(chipSelect);

		// print the type and size of the first FAT-type volume
		uint32_t volumesize;
		Serialprint("\nVolume type is FAT");
		Serial.println(volume.fatType(), DEC);
		Serialprintln();
		
		volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
		volumesize *= volume.clusterCount();       // we'll have a lot of clusters
		volumesize *= 512;                            // SD card blocks are always 512 bytes 
		Serialprint("Volume size (Mbytes): ");
		volumesize /= 1024;
		Serialprintln(volumesize);
		// list all files in the card with date and size
		root.ls(LS_R | LS_DATE | LS_SIZE);
		Serialprintln();
	}

	bool GetFile(char *filename){
		file.open(root, filename, O_READ);
		if (!file.isFile()) {
			Serialprintln("ERROR - Can't find file!");
			return false;  // can't find index file
		}
		else
		{
			return true;
		}

	}

private:
	int chipSelect;
	Sd2Card card;
	SdVolume volume;
	SdFile root;

};



// How big our line buffer should be. 100 is plenty!
#define BUFSIZ 100
#define TIMEOUTMS 2000

#define outputEthernetPin 10
#define outputSdPin 4
#define outputLcdPin 8

SDCard *card;

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 2, 120); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80

//File webFile;

void setup()
{

	// Open serial communications and wait for port to open:
	Serialbegin(9600);
	
	// disable w5100 SPI while starting SD
	pinMode(outputEthernetPin, OUTPUT);
	digitalWrite(outputEthernetPin, HIGH);

	card = new SDCard();
	card->initRoot(outputSdPin);
	//card->WebFileExists();


	Ethernet.begin(mac, ip);  // initialize Ethernet device
	server.begin();           // start to listen for clients

	// make sure that the default chip select pin is set to
	// output, even if you don't use it:
	/*pinMode(outputEthernetShieldSDPin, OUTPUT);*/
	//digitalWrite(outputEthernetShieldSDPin, HIGH);

	//pinMode(outputEthernetShieldEthernetPin, OUTPUT);



}

void loop()
{

	checkWebRequest();

	card->SDWriteLogPins("2015-01-12 12:00:01");
	card->SDGetCardInfo();

	delay(3000);

}


void checkWebRequest()
{
	char *IndexFile = "index.htm";
	char *DataFile = "datalog.txt";
	char clientline[BUFSIZ];
	int index = 0;
	unsigned long timeoutStart = 0;



	EthernetClient client = server.available();  // try to get client
	if (client) {  // got client?
		Serialprintln("Client!");
		timeoutStart = millis();
		while (client.connected()) {
			//Serialprintln("Client Connected!");
			if (client.available()) {   // client data available to read

				if (millis() - timeoutStart > TIMEOUTMS){
					client.println("HTTP/1.1 404 Not Found");
					client.println("Content-Type: text/html");
					client.println("Connection: close");
					client.println();
					client.println("<html><head></head><body><h2>Timeout reached</h2><body></html>");
					//delay(1);
					//client.stop(); // close the connection
					break;
				}


				char c = client.read(); // read 1 byte (character) from client
				// If it isn't a new line, add the character to the buffer
				Serialprint(c);
				if (c != '\n' && c != '\r') {
					clientline[index] = c;
					index++;
					// are we too big for the buffer? start tossing out data
					if (index >= BUFSIZ)
						index = BUFSIZ - 1;

					// continue to read more data!					
					continue; //go back to begining of while loop!
				}
				// got a \n or \r new line, which means the string is done
				/*Examples:
				GET / HTTP/1.1
				GET /himidity HTTP/1.1
				GET /temperature HTTP/1.1
				GET /log HTTP/1.1
				*/
				clientline[index] = 0; //terminate string
				if (strstr(clientline, "GET / ") != 0) {  //Returns Index file!
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println("Connection: close");
					client.println();

					// send web page   
					if (card->GetFile(IndexFile))
					{
						int16_t inchar = card->file.read();
						Serialprintln(inchar);
						Serialprintln("Got File!!");
						while (inchar >= 0) {
							client.print((char)inchar); // send web page to client
							inchar = card->file.read();
						}
						card->file.close();
						
					}
					else{
						Serialprintln("Can not read Index!");
					}
					break;
				}
				if (strstr(clientline, "GET /logfile ") != 0) {  //Returns size of log file file!
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: application/json");
					client.println("Connection: close");		
					client.println();
					client.println(5);
					/*if (card->GetFile(DataFile))
					{
						card->file.open(
						client.print("{size:");
						client.print(String(card->file..fileSize(), DEC));
						client.print("b}");
						client.println();
						card->file.close();
					}
					else{
						Serialprintln("Can not read Data!");
					}*/
					break;
				}
				else{
					// everything else is a 404
					client.println("HTTP/1.1 404 Not Found");
					client.println("Content-Type: text/html");
					client.println("Connection: close");
					client.println();
					client.println();
					client.println("<html><head></head><body><h2>File Not Found!</h2><body></html>");
					//delay(1);
					//client.stop(); // close the connection
					break;
				}
			}
		} // end if (client.available())
	} // end while (client.connected())
	delay(1);      // give the web browser time to receive the data
	client.stop(); // close the connection
}


/* Temp functions */
// comment out the next line to eliminate the Serial.print stuff
// saves about 1.6K of program memory
#define ServerDEBUG 1
void Serialbegin(unsigned long val){
#ifdef ServerDEBUG 
	Serial.begin(val);
	while (!Serial) {
		; // wait for serial port to connect. Needed for Leonardo only
	}
#endif
}
void Serialprint(char* val){ 
#ifdef ServerDEBUG 
	Serial.print(val);
#endif
}
void Serialprint(char val){
#ifdef ServerDEBUG 
	Serial.print(val);
#endif
}
void Serialprint(const String &val){
#ifdef ServerDEBUG 
	Serial.print(val);
#endif
}
void Serialprint(unsigned int val1, int val2){
#ifdef ServerDEBUG 
	Serial.print(val1, val2);
#endif
}
void Serialprintln(char* val){
#ifdef ServerDEBUG 
	Serial.println(val);
#endif
}
void Serialprintln(uint32_t val){
#ifdef ServerDEBUG 
	Serial.println(val);
#endif
}
void Serialprintln(){
#ifdef ServerDEBUG 
	Serial.println();
#endif
}
void Serialprintln(const String &val){
#ifdef ServerDEBUG 
	Serial.print(val);
#endif
}