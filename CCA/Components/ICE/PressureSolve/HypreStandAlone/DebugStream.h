#ifndef __DEBUGSTREAM_H__
#define __DEBUGSTREAM_H__

#include "Macros.h"
#include <iostream>
#include <iomanip>
#include <sstream>
using std::string;

class DebugStream;
class DebugBuf;
extern std::string lineHeader(const Counter& indent);

class DebugBuf: public std::streambuf {
 public:
  DebugStream* owner;      // points to the DebugStream that instantiated me

  DebugBuf(void);
  ~DebugBuf(void);
  int overflow(int ch);
  std::streamsize xsputn (const char* s,
                          std::streamsize num);
 private:
   bool _lineBegin;
};

class DebugStream: public std::ostream {        
 public:
  DebugStream(void);
  DebugStream(const std::string& name,
    bool defaulton = true);
  ~DebugStream(void);

  bool    active(void) const { return _isactive; };
  int     getVerboseLevel(void) const { return _verboseLevel; }
  int     getLevel(void) const { return _level; }
  Counter getIndent(void) const { return _indent; }

  void setActive(const bool active);
  void setVerboseLevel(const int verboseLevel) { _verboseLevel = verboseLevel; }
  void setLevel(const int level) { _level = level; }
  void indent(void);
  void unindent(void);
  
  std::ostream *outstream;   // ostream that output redirected to. default: cout
  
 private: 
  std::string _name;         // Uniquely identifies me
  DebugBuf*   _dbgbuf;       // Buffer that is used for output redirection
  bool        _isactive;     // If false, all input is ignored
  int         _verboseLevel; // Verbose level for printouts, badly implemented...
  int         _level;        // Current level for printouts, badly implemented...
  Counter     _indent;       // # spaces for line indentation 
};
    
#endif // __DEBUGSTREAM_H__
