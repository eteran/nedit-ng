#!/bin/bash


while read LINE; do
	FILE=$(echo $LINE | cut -d: -f1);
	KEYWORD=$(echo $LINE | cut -d: -f2 | sed -e 's/<h1>//g' | sed -e 's:</h1>::g');

	
	echo "<keyword name=\"$KEYWORD\" ref=\"./$FILE\"/>"
	
done < <(egrep -o '<h1>(.+)</h1>' *.html)
