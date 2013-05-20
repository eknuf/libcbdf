/*
 * cbdf.cpp
 *
 *  Created on: Apr 2, 2013
 *      Author: funke
 */

#include <cbdf.h>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/crc.hpp>

#ifdef WITH_LZMA
#include "lzma.hpp"
#endif

#ifdef WITH_LZO
#include "lzo.hpp"
#endif

#ifdef WITH_BOOST_UUID
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#else
#include <uuid++.hh>
#endif 


namespace boostIO = boost::iostreams;


#pragma pack(4) // Enforce 32 Bit alignment for ondisk format
struct cbdf::cbdfFileHeader_t{
    uint32_t openTag;           //0xCBDFCBDF
    uint64_t timeStart;         //Unix time. Cast to time_t if neccessary
    uint64_t features;          //Bitset of cbdf features (see documentation)
    char uuid[36];              //UUID for given file stored as string (xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx)
    uint32_t closeTag;          //0xCBDFCBDF
  };

  struct cbdf::cbdfFileTrailer_t{
    uint32_t openTag;           //0xFDBCFDBC
    uint64_t timeStop;          //Unix time. Cast to time_t if neccessary
    uint64_t features;          //Bitset of cbdf features (see documentation)
    char uuid[36];              //UUID for given file for given file stored as string (xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx)
    uint32_t closeTag;          //0xFDBCFDBC
  };

  struct cbdf::cbdfEventHeader_t{
    uint32_t openTag;           //0xCBEDCBED
    uint64_t eventNumber;       //Linear throughout the file
    uint64_t userFlags;         //User defined Flags (e.g. Eventtype)
    uint64_t eventSize;         //Payload size in bytes
    uint32_t closeTag;          //0xCBEDCBED
  };

  struct cbdf::cbdfEventTrailer_t{
    uint32_t openTag;           //0xDEBCDEBC
    uint32_t crc32;             //CRC32 checksum of payload
    uint64_t eventSize;         //Payload size in bytes
    uint32_t closeTag;          //0xDEBCDEBC
  };


  struct cbdf::cbdfBankHeader_t{
    char name[12];              // \0 terminated string of up to 11 characters
    uint16_t userFlags;         // User defined bank flags
    uint32_t size;              // Payloadsize of the bank in bytes
  };
#pragma pack() // reset padding to compiler defaults


// Utility functions

uint64_t cbdf::rEventSize()
{
  return (sizeof(cbdfEventHeader_t)+rEventHeader->eventSize+sizeof(cbdfEventTrailer_t));
}
uint64_t cbdf::wEventSize()
{
  return (sizeof(cbdfEventHeader_t)+wEventHeader->eventSize+sizeof(cbdfEventTrailer_t));
}

uint32_t cbdf::badEventHeader()
{
  return ((rEventHeader->openTag & rEventHeader->closeTag)^0xcbedcbed);
}
uint32_t cbdf::badEventTrailer()
{
  return ((rEventTrailer->openTag & rEventTrailer->closeTag)^0xdebcdebc);
}

uint32_t cbdf::headerTrailerMismatch()
{
  return (rEventHeader->eventSize^rEventTrailer->eventSize);
}


//Private methods

int cbdf::writeFileHeader()
{
  #ifdef WITH_BOOST_UUID
  boost::uuids::basic_random_generator<boost::mt19937> gen;
  boost::uuids::uuid _uuid = gen();
  strncpy(wFileHeader->uuid, (boost::uuids::to_string(_uuid)).c_str(), 36);
  strncpy(wFileTrailer->uuid, (boost::uuids::to_string(_uuid)).c_str(), 36);
  #else
  uuid _uuid;
  _uuid.make(4);
  strncpy(wFileHeader->uuid, _uuid.string(), 36);
  strncpy(wFileTrailer->uuid, _uuid.string(), 36);
  #endif
  //Prepare Header
  wFileHeader->openTag = 0xCBDFCBDF;
  wFileHeader->closeTag = 0xCBDFCBDF;
  wFileHeader->timeStart = time(NULL);
  wFileHeader->features = 0; //TODO Define feature bitset

  //Prepare Trailer
  wFileTrailer->openTag = 0xFDBCFDBC;
  wFileTrailer->closeTag = 0xFDBCFDBC;
  wFileTrailer->features = 0;

  ((boostIO::filtering_ostream*)cbdfOutFile)->write((const char *) wFileHeader, sizeof(cbdfFileHeader_t));
  return 0;
}
int cbdf::writeFileTrailer()
{
  wFileTrailer->timeStop = time(NULL);
  ((boostIO::filtering_ostream*)cbdfOutFile)->write((const char *) wFileTrailer, sizeof(cbdfFileTrailer_t));
  return 0;
}

uint32_t cbdf::crc32()
{
  boost::crc_32_type _result;
  _result.process_bytes(payloadBase, payloadSize);
  return _result.checksum();
}

int cbdf::resizeEventbuffer()
{
  eventBufferSize *= 2;
  eventBufferBase = (char*) realloc(eventBufferBase, eventBufferSize);
  if (eventBufferBase == NULL)
    return -1;

  wEventHeader = (cbdfEventHeader_t*) eventBufferBase;
  rEventHeader = (cbdfEventHeader_t*) eventBufferBase;
  payloadBase = eventBufferBase + sizeof(cbdfEventHeader_t);
  payloadPtr = payloadBase + payloadSize;
  return 0;
}

int cbdf::readFileHeader()
{
  ((boostIO::filtering_istream*)cbdfInFile)->read((char*) rFileHeader, sizeof(cbdfFileHeader_t));
  return checkFileHeader();
}

int cbdf::checkFileHeader()
{
  if ((rFileHeader->openTag & rFileHeader->closeTag) == 0xcbdfcbdf)
  {
    std::cerr << "Found file header" << std::endl;
    return 0;
  }
  std::cerr << "Fileheader mismatch" << std::endl;
  return CBDF_FILE_HEADER_ERROR;
}

int cbdf::checkFileTrailer()
{
  if ((rFileTrailer->openTag & rFileTrailer->closeTag) == 0xfdbcfdbc)
  {
    std::cerr << "End of file detected, found file trailer" << std::endl;
    return CBDF_EOF;
  }
  std::cerr << "Unexpected end of file, no file trailer found" << std::endl;
  return CBDF_UNEXPECTED_EOF;
}

int cbdf::checkEvent()
{

  if ((rEventHeader->openTag == 0xcbedcbed) && (rEventHeader->closeTag == 0xcbedcbed) && (rEventTrailer->openTag == 0xdebcdebc) && (rEventTrailer->closeTag == 0xdebcdebc) && (rEventHeader->eventSize == rEventTrailer->eventSize))
    return 0;
  else
    return CBDF_EVENT_HEADER_TRAILER_MISMATCH;
}

//Public methods

cbdf::cbdf(uint64_t _eventBufferSize)
{
  // Allocate event buffer
  eventBufferBase = (char*) calloc(_eventBufferSize, 1);
  
  // Allocate data structures
  wEventTrailer = new cbdfEventTrailer_t;
  wFileHeader   = new cbdfFileHeader_t;
  rFileHeader   = new cbdfFileHeader_t;
  wFileTrailer  = new cbdfFileTrailer_t;
 

  //Write static tags to buffer
  wEventHeader = (cbdfEventHeader_t*) eventBufferBase;
  wEventHeader->openTag = 0xCBEDCBED;
  wEventHeader->eventNumber=0;
  wEventHeader->userFlags=0;
  wEventHeader->eventSize=0;
  wEventHeader->closeTag = 0xCBEDCBED;
  wEventTrailer->openTag = 0xDEBCDEBC;
  wEventTrailer->crc32 = 0;
  wEventTrailer->eventSize=0;
  wEventTrailer->closeTag = 0xDEBCDEBC;

  payloadBase = eventBufferBase + sizeof(cbdfEventHeader_t);
  payloadPtr = payloadBase;
  eventBufferSize = _eventBufferSize;
  payloadSize = 0;
  bytesBuffered = 0;
  currentUserFlags=0;

  //Initialize variables to fix compiler warnings
  cbdfInFile = NULL;
  cbdfOutFile = NULL;
  wBankHeader = NULL;
  fileAccessMode = readMode;
  eventBuffered = false;

}

int cbdf::fileOpen(std::string filename, fileAccessMode_t mode, compressionType_t compr)
{
  int _nfilters = 0;
  currentFileName=filename;
  switch (mode)
  {
  case (readMode):
    cbdfInFile = (void*) new boostIO::filtering_istream();
    switch (compr)
    {
    case (gzip):
      ((boostIO::filtering_istream*)cbdfInFile)->push(boostIO::gzip_decompressor());
      _nfilters++;
      break;
    case (bzip2):
      ((boostIO::filtering_istream*)cbdfInFile)->push(boostIO::bzip2_decompressor());
      _nfilters++;
      break;
    case (xz):
      #ifdef WITH_LZMA
      ((boostIO::filtering_istream*)cbdfInFile)->push(boostIO::lzma_decompressor());
      _nfilters++;
      #else
      std::cerr << "No LZMA support enabled at compile time -- Exiting\n";
      exit(-1);
      #endif
      break;
    case (lzo):
      #ifdef WITH_LZO
      ((boostIO::filtering_istream*)cbdfInFile)->push(boostIO::lzo_decompressor());
      _nfilters++;
      #else 
      std::cerr << "No LZO support enabled at compile time -- Exiting\n";
      exit(-1);
      #endif
      break;
    default:
      break;
    }
    ((boostIO::filtering_istream*)cbdfInFile)->push(boostIO::file_source(currentFileName));
    if (((boostIO::filtering_istream*)cbdfInFile)->component<boostIO::file_source>(1)->is_open())
    {
      fileAccessMode = readMode;
      readFileHeader();
      rEventHeader = (cbdfEventHeader_t*) eventBufferBase;
      payloadBase = eventBufferBase + sizeof(cbdfEventHeader_t);
    }
    else
    {
      return -1;
    }
    break;
  case (writeMode):
    cbdfOutFile = (void*) new boostIO::filtering_ostream;
    switch (compr)
    {
    case (gzip):
      ((boostIO::filtering_ostream*)cbdfOutFile)->push(boostIO::gzip_compressor());
      currentFileName = currentFileName + ".gz";
      _nfilters++;
      break;
    case (bzip2):
      ((boostIO::filtering_ostream*)cbdfOutFile)->push(boostIO::bzip2_compressor());
      currentFileName = currentFileName + ".bz2";
      _nfilters++;
      break;
    case (xz):
      #ifdef WITH_LZMA
      ((boostIO::filtering_ostream*)cbdfOutFile)->push(boostIO::lzma_compressor());
      currentFileName = currentFileName + ".xz";
      _nfilters++;
      #else
      std::cerr << "LZMA support not enabled at compile time, writing uncompressed!\n";
      #endif
      break;
    case (lzo):
      #ifdef WITH_LZO
      ((boostIO::filtering_ostream*)cbdfOutFile)->push(boostIO::lzo_compressor());
      currentFileName = currentFileName + ".lzo";
      _nfilters++;
      #else
      std::cerr << "LZO support not enabled at compile time, writing uncompressed!\n";
      #endif 
      break;
    default:
      break;
    }
    ((boostIO::filtering_ostream*)cbdfOutFile)->push(boostIO::file_sink(currentFileName));
    if (((boostIO::filtering_ostream*)cbdfOutFile)->component<boostIO::file_sink>(_nfilters)->is_open())
    {
      fileAccessMode = writeMode;
      writeFileHeader();
    }
    else
    {
      return -1;
    }
    break;
  default:
    break;
  }
  // Set Eventnumber to first event
  currentEventnumber = 1;
  return 0;
}

int cbdf::fileClose()
{
  switch (fileAccessMode)
  {
  case (readMode):
    ((boostIO::filtering_istream*)cbdfInFile)->pop();
    delete (boostIO::filtering_istream*) cbdfInFile;
    break;
  case (writeMode):
    writeFileTrailer();
    ((boostIO::filtering_ostream*)cbdfOutFile)->pop();
    delete (boostIO::filtering_ostream*) cbdfOutFile;
    break;
  default:
    break;
  };

  return 0;
}

// Write access methods
int cbdf::clearEvent()
{
  payloadSize = 0;
  payloadPtr = payloadBase;
  bankMap.clear();
  return 0;
}

int cbdf::writeEvent()
{
//Fill in header values
  wEventHeader->eventNumber = currentEventnumber;
  wEventHeader->userFlags = currentUserFlags;
  wEventHeader->eventSize = payloadSize;

//Calculate CRC and add trailer to buffer
  wEventTrailer->eventSize = payloadSize;
  wEventTrailer->crc32 = crc32();
  memcpy(payloadPtr, wEventTrailer, sizeof(cbdfEventTrailer_t));

//Write buffer to file
  ((boostIO::filtering_ostream*)cbdfOutFile)->write((const char *) eventBufferBase, wEventSize());
//Prepare next event
  currentEventnumber++;
  clearEvent();

  return 0;
}

int cbdf::setEventUserFlags(uint64_t userFlags)
{
  if (fileAccessMode == writeMode)
  {
    currentUserFlags = userFlags;
    return 0;
  }
  else
  {
    return -1;
  }

}

int cbdf::addBank(const char* name, uint16_t userFlags, char* dataPointer, uint32_t dataSize)
{
  uint32_t _bankSize = sizeof(cbdfBankHeader_t) + dataSize;
  while ((payloadSize + sizeof(cbdfEventHeader_t) + sizeof(cbdfEventTrailer_t) + _bankSize) > eventBufferSize)
    resizeEventbuffer();
  wBankHeader = (cbdfBankHeader_t *) payloadPtr;
  for (int i = 0; i < 12; i++)
    wBankHeader->name[i] = name[i];
  wBankHeader->userFlags = userFlags;
  wBankHeader->size = dataSize;
  memcpy(payloadPtr + sizeof(cbdfBankHeader_t), dataPointer, dataSize);
  payloadPtr += _bankSize;
  payloadSize += _bankSize;

  return 0;
}

int cbdf::addRawData(char* bankPointer, uint32_t bankSize)
{
  while ((payloadSize + sizeof(cbdfEventHeader_t) + sizeof(cbdfEventTrailer_t) + bankSize) > eventBufferSize)
    resizeEventbuffer();
  memcpy(payloadPtr, bankPointer, bankSize);
  payloadPtr += bankSize;
  payloadSize += bankSize;
  return 0;
}

int cbdf::skipEvents(int toSkip)
{
  for(int i=0; i < toSkip ; i++)
  {
    ((boostIO::filtering_istream*)cbdfInFile)->read((char *) eventBufferBase, sizeof(cbdfEventHeader_t));
    eventBuffered=false;
    if (((boostIO::filtering_istream*)cbdfInFile)->good())
    {
      if (badEventHeader())
      {
	// Check if we reached the end of the datafile
	if (rEventHeader->openTag == 0xfdbcfdbc)
	{
	  // Read in complete header
	  ((boostIO::filtering_istream*)cbdfInFile)->read((char *) payloadBase, sizeof(cbdfFileTrailer_t) - sizeof(cbdfEventHeader_t));
	  rFileTrailer = (cbdfFileTrailer_t*) eventBufferBase;
	  return checkFileTrailer();
	}
	std::cerr << "Bad header\n";
	printEvent();
	return CBDF_EVENT_HEADER_NOT_FOUND;
      }
      while (rEventSize() > eventBufferSize)
	resizeEventbuffer();
      ((boostIO::filtering_istream*)cbdfInFile)->read((char *) payloadBase, rEventHeader->eventSize + sizeof(cbdfEventTrailer_t));
      eventBuffered=true;
      payloadPtr = payloadBase;
      rEventTrailer = (cbdfEventTrailer_t*) (payloadBase + payloadSize);
      if (badEventTrailer() || headerTrailerMismatch())
      {
	std::cerr << "Bad trailer\n";
	printEvent();
	return CBDF_EVENT_HEADER_TRAILER_MISMATCH;
      }
    }else
    {
      return CBDF_UNEXPECTED_EOF;
    }
  }
  return readEvent();
}

int cbdf::readEvent()
{
  cbdfBankMapEntry_t _currentBank;
  std::string _currentBankName;
  uint32_t _sizeRead = 0;
  bankMap.clear();
  ((boostIO::filtering_istream*)cbdfInFile)->read((char *) eventBufferBase, sizeof(cbdfEventHeader_t));
  eventBuffered=false;
  if (((boostIO::filtering_istream*)cbdfInFile)->good())
  {
    if (badEventHeader())
    {
      // Check if we reached the end of the datafile
      if (rEventHeader->openTag == 0xfdbcfdbc)
      {
        // Read in complete header
        ((boostIO::filtering_istream*)cbdfInFile)->read((char *) payloadBase, sizeof(cbdfFileTrailer_t) - sizeof(cbdfEventHeader_t));
        rFileTrailer = (cbdfFileTrailer_t*) eventBufferBase;
        return checkFileTrailer();
      }
      std::cerr << "Bad header\n";
      printEvent();
      return CBDF_EVENT_HEADER_NOT_FOUND;
    }
    currentEventnumber = rEventHeader->eventNumber;
    currentUserFlags = rEventHeader->userFlags;
    payloadSize = rEventHeader->eventSize;
    while (rEventSize() > eventBufferSize)
      resizeEventbuffer();
    ((boostIO::filtering_istream*)cbdfInFile)->read((char *) payloadBase, rEventHeader->eventSize + sizeof(cbdfEventTrailer_t));
    eventBuffered=true;
    payloadPtr = payloadBase;
    rEventTrailer = (cbdfEventTrailer_t*) (payloadBase + payloadSize);
    if (badEventTrailer() || headerTrailerMismatch())
    {
      std::cerr << "Bad trailer\n";
      printEvent();

      return CBDF_EVENT_HEADER_TRAILER_MISMATCH;
    }

    if (crc32() != rEventTrailer->crc32)
    {
      std::cerr << "CRC32 mismatch Payload:" << crc32() << " Event: " << rEventTrailer->crc32 << std::endl;
      
      return CBDF_EVENT_CRC_ERROR;
    }
    //Fill Bank Map
    while (_sizeRead < payloadSize)
    {
      rBankHeader = (cbdfBankHeader_t *) payloadPtr;
      _currentBankName.assign(rBankHeader->name);
      memcpy(&_currentBank, rBankHeader, sizeof(cbdfBankHeader_t));
      payloadPtr += sizeof(cbdfBankHeader_t);
      _currentBank.dataPtr = payloadPtr;
      bankMap.insert(bankPair_t(_currentBankName, _currentBank));
      payloadPtr += _currentBank.size;
      _sizeRead += sizeof(cbdfBankHeader_t) + _currentBank.size;
    }
    // Check if the whole payload was consumed otherwise clear bankmap
    if (_sizeRead != payloadSize)
    {
      bankMap.clear();
      return CBDF_BANK_ERROR;
    }
    return 0;
  }
  else
  {
    bankMap.clear();
    std::cerr << "EOF detected" << std::endl;
    return CBDF_UNEXPECTED_EOF;
  }
}

cbdf::cbdfBankMapEntry_t cbdf::getBank(std::string bankName)
{
  if (bankMap.find(bankName) != bankMap.end())
  {
    return bankMap.find(bankName)->second;
  }
  else
  {
    std::cerr << "Bank: " << bankName << " not found" << std::endl;
    cbdfBankMapEntry_t _dummyBank;
    bzero(&_dummyBank, sizeof(_dummyBank));
    return _dummyBank;
  }
}

cbdf::bankMapIt_t cbdf::getBanks()
{
  return bankMap.begin();
}

int cbdf::getRawData(char* dataPointer, uint64_t &dataSize)
{
  dataPointer = eventBufferBase + sizeof(cbdfEventHeader_t);
  dataSize = rEventHeader->eventSize;
  return 0;
}

uint64_t cbdf::getEventNumber()
{
  return currentEventnumber;
}

uint64_t cbdf::getEventUserFlags()
{
  return currentUserFlags;
}

uint64_t cbdf::getEventSize()
{
  return payloadSize;
}

char* cbdf::getUuid()
{
  if(fileAccessMode==readMode)
    return rFileHeader->uuid;
  else
    return wFileHeader->uuid;
}

std::string cbdf::getFileName()
{
  return currentFileName;
}

/*
 * Error handling functions
 */

int cbdf::scanForNextEvent()
{
  //TODO Here be error handling
  if(eventBuffered)
  {
    
  }else
  {
    
  }
  return 0;
}



/*
 * Debug function to pretty print event data
 */

void cbdf::printFileHeader()
{

  std::string _uuid = rFileHeader->uuid;
  std::cout << ctime((time_t*) rFileHeader->timeStart) << " " << _uuid << std::endl << std::endl;
}

void cbdf::printEvent()
{
  std::cout << "Header:\n" << std::hex << rEventHeader->openTag << " " << rEventHeader->closeTag << std::endl;
  std::cout << "Trailer:\n" << std::hex << rEventTrailer->openTag << " " << rEventTrailer->closeTag << std::endl;
}

void cbdf::printBanks()
{
  bankMap_t::iterator _itBankMap = getBanks();
  for (_itBankMap = bankMap.begin(); _itBankMap != bankMap.end(); _itBankMap++)
    hexDump(_itBankMap->second, ascii);
}

void cbdf::hexDump(cbdfBankMapEntry_t bank, dumpMode_t dumpMode)
{
  std::cout << "Start of Bank " << bank.name << " (size " << bank.size << " bytes in Eventnr. " << getEventNumber() << " --------------- \n\n";

  switch (dumpMode)
  {
  case (hex):
  {
    uint32_t *uPointer = (uint32_t *) (bank.dataPtr);
    for (int i = 0; i < bank.size / 4; i++)
    {
      std::cout << std::setw(8) << std::setfill('0') << std::hex << *uPointer++ << " ";
      if (i % 8 == 7)
        std::cout << std::endl;
    }
  }
    break;
  case (ascii):
  {
    char *cPointer = bank.dataPtr;
    for (int i = 0; i < bank.size; i++)
    {
      std::cout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int) *cPointer++ << " ";
      if (i % 32 == 31)
        std::cout << std::endl;
    }
  }
    break;
  default:
    std::cout << "Unsupported dump format please choose either cbdf::hex or cbdf::ascii\n";
  }
  std::cout << std::endl << std::endl;
  std::cout << "End of Bank " << bank.name << " (size " << bank.size << " bytes) --------------- \n\n";
}

cbdf::~cbdf()
{
// TODO Auto-generated destructor stub
}

