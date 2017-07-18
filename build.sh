#!/bin/bash
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *#
#*                                                                         *#
#*  baselib: a library implementing several simple utilities for C         *#
#*  Copyright (C) 2017  LeqxLeqx                                           *#
#*                                                                         *#
#*  This program is free software: you can redistribute it and/or modify   *#
#*  it under the terms of the GNU General Public License as published by   *#
#*  the Free Software Foundation, either version 3 of the License, or      *#
#*  (at your option) any later version.                                    *#
#*                                                                         *#
#*  This program is distributed in the hope that it will be useful,        *#
#*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *#
#*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *#
#*  GNU General Public License for more details.                           *#
#*                                                                         *#
#*  You should have received a copy of the GNU General Public License      *#
#*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *#
#*                                                                         *#
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *#

if [[ !(-d bin) ]] ; then
  mkdir bin
  if [[ $? != 0 ]] ; then
    echo "Failed to create directory 'bin' build.sh terminated"
    exit 1
  fi
fi

rm -f bin/*.o

gcc src/*.c -g -c -fPIC
mv *.o bin/
gcc bin/*.o -shared -o bin/libbaselib.so
