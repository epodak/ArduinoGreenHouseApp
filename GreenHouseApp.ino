
#include <SPI.h>
#include <SD.h>



class SDCard
{
public:
	void init(int chipSelected){
		chipSelect = chipSelected;
		// see if the card is present and can be initialized:
			if (!SD.begin(chipSelect))
			{
				Serial.println("Card failed, or not present");
				// don't do anything more:
				return;
			}
		Serial.println("card initialized.");
	};

	void SDWriteLogPins(String DateTimeStamp)
	{



		/*
		SD card datalogger
		This example shows how to log data from three analog sensors
		to an SD card using the SD library.
		The circuit:
		* analog sensors on analog ins 0, 1, and 2
		* SD card attached to SPI bus as follows:
		** MOSI - pin 11
		** MISO - pin 12
		** CLK - pin 13
		** CS - pin 4

		created  24 Nov 2010
		modified 9 Apr 2012
		by Tom Igoe

		This example code is in the public domain.

		*/
		int numberOfPins = 6; //number of analog pins to log
		//Serial.print("Initializing SD card...");
		// make a string for assembling the data to log:
		String dataString = DateTimeStamp + ":";

		// read three sensors and append to the string:
		for (int analogPin = 0; analogPin < numberOfPins; analogPin++) {
			int sensor = analogRead(analogPin);
			dataString += String(sensor);
			if (analogPin < (numberOfPins - 1)) {
				dataString += ",";
			}
		}
		// open the file. note that only one file can be open at a time,
		// so you have to close this one before opening another.
		File dataFile = SD.open("datalog.txt", FILE_WRITE);

		// if the file is available, write to it:
		if (dataFile) {
			dataFile.println(dataString);
			dataFile.close();
			// print to the serial port too:
			Serial.println(dataString);
		}
		else {
			// if the file isn't open, pop up an error:
			Serial.println("error opening datalog.txt");
		}


	}

	void SDGetCardInfo()
	{
		Sd2Card card;
		SdVolume volume;
		SdFile root;


		// we'll use the initialization code from the utility libraries
		// since we're just testing if the card is working!
		if (!card.init(SPI_HALF_SPEED, chipSelect)){
			Serial.println("initialization failed. Things to check:");
			/*Serial.println("* is a card is inserted?");
			Serial.println("* Is your wiring correct?");
			Serial.println("* did you change the chipSelect pin to match your shield or module?");*/
			return;
		}
		else {
			Serial.println("Wiring is correct and a card is present.");
		}

		// print the type of card
		Serial.print("\nCard type: ");
		switch (card.type()) {
		case SD_CARD_TYPE_SD1:
			Serial.println("SD1");
			break;
		case SD_CARD_TYPE_SD2:
			Serial.println("SD2");
			break;
		case SD_CARD_TYPE_SDHC:
			Serial.println("SDHC");
			break;
		default:
			Serial.println("Unknown");
		}

		// Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
		if (!volume.init(card)) {
			Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
			return;
		}


		// print the type and size of the first FAT-type volume
		uint32_t volumesize;
		Serial.print("\nVolume type is FAT");
		Serial.println(volume.fatType(), DEC);
		Serial.println();

		volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
		volumesize *= volume.clusterCount();       // we'll have a lot of clusters
		volumesize *= 512;                            // SD card blocks are always 512 bytes
		Serial.print("Volume size (bytes): ");
		Serial.println(volumesize);
		Serial.print("Volume size (Kbytes): ");
		volumesize /= 1024;
		Serial.println(volumesize);
		Serial.print("Volume size (Mbytes): ");
		volumesize /= 1024;
		Serial.println(volumesize);


		Serial.println("\nFiles found on the card (name, date and size in bytes): ");
		root.openRoot(volume);

		// list all files in the card with date and size
		root.ls(LS_R | LS_DATE | LS_SIZE);

	}

private:
	int chipSelect;
};


// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 4;
const int outputEthernetShieldSDPin = 10; //to turn on SD library
SDCard *card;

void setup()
{
	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for Leonardo only
	}

	card = new SDCard();
	card->init(chipSelect);

	// make sure that the default chip select pin is set to
	// output, even if you don't use it:
	pinMode(outputEthernetShieldSDPin, OUTPUT);
	digitalWrite(outputEthernetShieldSDPin, HIGH);
	



}

void loop()
{

	card->SDWriteLogPins("2015-01-12 12:00:01");
	card->SDGetCardInfo();

	delay(3000);

}



