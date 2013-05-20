/*
 * cbdf.h
 *
 *  Created on: Apr 2, 2013
 *      Author: funke
 */

#ifndef CBDF_H_
#define CBDF_H_

#include <stdint.h>
#include <exception>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>

// Define Error Codes

#define CBDF_EOF 1
#define CBDF_FILE_HEADER_ERROR -1
#define CBDF_EVENT_HEADER_NOT_FOUND -2
#define CBDF_EVENT_HEADER_TRAILER_MISMATCH -3
#define CBDF_EVENT_CRC_ERROR -4
#define CBDF_UNEXPECTED_EOF -5
#define CBDF_BANK_ERROR -6



class cbdf
{
private:

  // Structural components

  struct cbdfFileHeader_t;
  cbdfFileHeader_t *wFileHeader,*rFileHeader;
  struct cbdfFileTrailer_t;
  cbdfFileTrailer_t *wFileTrailer,*rFileTrailer;
  struct cbdfEventHeader_t;
  cbdfEventHeader_t *wEventHeader,*rEventHeader;
  struct cbdfEventTrailer_t;
  cbdfEventTrailer_t *wEventTrailer,*rEventTrailer;
  struct cbdfBankHeader_t;
  cbdfBankHeader_t *wBankHeader,*rBankHeader;
  
  // Buffers

  char* eventBufferBase;
  char* readBufferPtr;
  char* payloadBase;
  char* payloadPtr;

  // Internal state variables;

  uint64_t currentEventnumber;
  uint64_t currentUserFlags;
  uint64_t eventBufferSize;
  uint64_t payloadSize;
  uint64_t bytesBuffered;
  std::string currentFileName;
  
  bool eventBuffered;

  // File I/O streams

  void *cbdfInFile; // really boost::iostreams::filtering_istream *cbdfInFile;
  void *cbdfOutFile; // really boost::iostreams::filtering_ostream *cbdfOutFile;

  // Utility functions

  uint64_t rEventSize();
  uint64_t wEventSize();

  uint32_t badEventHeader();
  uint32_t badEventTrailer();

  uint32_t headerTrailerMismatch();


  //Private Methods

  int resizeEventbuffer();

  int writeFileHeader();
  int writeFileTrailer();

  int readFileHeader();
  int readFileTrailer();

  // Integrity checks
  uint32_t crc32();
  int checkFileHeader();
  int checkFileTrailer();

  int checkEventHeader();
  int checkEventTrailer();

  int checkEvent();



public:

  // Enums to enhance readability of code

  enum fileAccessMode_t {readMode=0,writeMode=1};
  enum compressionType_t {none=0,gzip=1,bzip2=3,xz=4,lzo=5};
  enum dumpMode_t {ascii=0,hex=1};

  // Struct for public access to the event data

#pragma pack(4) // Enforce 32 Bit alignment for ondisk format
  struct cbdfBankMapEntry_t{
      char name[12];
      uint16_t userFlags;
      uint32_t size;
      char *dataPtr;
  };
#pragma pack() // reset padding to compiler defaults
  
  typedef std::pair<std::string,cbdfBankMapEntry_t> bankPair_t;
  typedef std::map<std::string, cbdfBankMapEntry_t> bankMap_t;
  typedef std::map<std::string, cbdfBankMapEntry_t>::iterator bankMapIt_t;
  bankMap_t bankMap;

  fileAccessMode_t fileAccessMode;

  // Constructor

  cbdf(uint64_t _eventBufferSize=1048576);

  // File Handling

  int fileOpen(std::string filename, fileAccessMode_t mode=readMode, compressionType_t=none );
  int fileClose();

  // Write access methods
  int clearEvent(); //Resets pointer of event buffer without incrementing the eventcounter
  int writeEvent(); //Write event and increment eventcounter;
  int setEventUserFlags(uint64_t userFlags);
  int addBank(const char* name, uint16_t userFlags, char* dataPointer, uint32_t dataSize);
  int addRawData(char* bankPointer, uint32_t bankSize);

  // Read access methods
  int readEvent();
  int skipEvents(int);
  cbdfBankMapEntry_t getBank(std::string bankName);
  bankMapIt_t getBanks();
  int getRawData(char* dataPointer, uint64_t &dataSize);
  uint64_t getEventNumber();
  uint64_t getEventUserFlags();
  uint64_t getEventSize();

  char* getUuid();
  std::string getFileName();
  
  // Error handling functions
  int scanForNextEvent();

  // Debug function
  void printFileHeader();
  void printEvent();
  void printBanks();
  void hexDump(cbdfBankMapEntry_t, dumpMode_t dumpMode=hex);

  virtual ~cbdf();
};

#endif /* CBDF_H_ */
