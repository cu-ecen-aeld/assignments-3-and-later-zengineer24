#!/bin/bash

if [ $# -ne 2 ]
then
	echo "Please specify a directory and a string to search for"
	exit 1

else
	filesdir=$1
	searchstr=$2
	if [ -d "$filesdir" ]
	then
		cd "$filesdir"
		fileCount=$(ls | wc -l)
		matchCount=$(grep -r "$searchstr" "$filesdir"/* | wc -l)
		echo "The number of files are ${fileCount} and the number of matching lines are ${matchCount}"
 
	else
		echo "Please provide a valid directory path."
		exit 1
	fi
fi
