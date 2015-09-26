#!/bin/sh
# $Id: version.sh,v 1.3 1997/04/24 12:27:53 vuori Exp $

if [ ! -s 'version' ]
then
echo "version file not found" > /dev/stderr
exit 1
fi

if [ ! -f 'compile' -o ! -s 'compile' ] 
then
echo "1" > compile
else
echo $((`cat compile` + 1)) > compile
fi
echo "#define VERS \"dt `cat version` #`cat compile`: `date` (`whoami`@`hostname`)\""
echo "#define VERSNUM \"dt-`cat version`\""
