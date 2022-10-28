/*
  DebugUtils.h - Simple debugging utilities.
  Copyright (C) 2011 Fabio Varesano <fabio at varesano dot net>

  Ideas taken from:
  http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1271517197

  This program is free software: you can redistribute it and/or modify
  it under the terms of the version 3 GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "ERROR: Please define #FIRMWARE_VERSION"
#endif

#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H



#ifdef DEBUG
#include <string>
const char compile_date[] = __DATE__ " " __TIME__;
String compile_date_modified(compile_date);
const char file_name[] = __FILE__;
String file_name_modified(file_name);

#include <WProgram.h>
#define DEBUG_PRINT(str)    \
  Serial.print(FIRMWARE_VERSION);   \
  Serial.print(" ");   \
  Serial.print(compile_date_modified.remove(17));   \
  Serial.print(" mS:");    \
  Serial.print(millis());     \
  Serial.print(": ");    \
  Serial.print(__PRETTY_FUNCTION__); \
  Serial.print(':');      \
  Serial.print(__LINE__);     \
  Serial.print(' ');      \
  Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

#endif
