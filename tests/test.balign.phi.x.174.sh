#!/bin/sh

OUTPUT_ID="phi.x.174";
OUTPUT_DIR="output/";
SAVE_DIR="save/";
TMP_DIR="tmp/";
RG=$OUTPUT_DIR"bfast.rg.file.$OUTPUT_ID.0.brg";

echo "      Running local alignment.";

for PAIRED_END in 0 1
do
	for SPACE in 0 1
	do
		if [ "$PAIRED_END" -eq "0" ]; then
			echo "        Testing -A "$SPACE;
		else
			echo "        Testing -A "$SPACE" -2";
		fi

		SCORING=$OUTPUT_DIR"scoring.$SPACE.txt";
		MATCHES=$OUTPUT_DIR"bfast.matches.file.$OUTPUT_ID.$SPACE.$PAIRED_END.bmf";

		# Run local alignment
		if [ "$PAIRED_END" -eq "0" ]; then
			CMD="../balign/balign -r $RG -m $MATCHES -x $SCORING -A $SPACE -o $OUTPUT_ID -d $OUTPUT_DIR -T $TMP_DIR";
		else
			CMD="../balign/balign -r $RG -m $MATCHES -x $SCORING -A $SPACE -2 -o $OUTPUT_ID -d $OUTPUT_DIR -T $TMP_DIR";
		fi
		$CMD 2> /dev/null > /dev/null; 
		# Get return code
		if [ "$?" -ne "0" ]; then
			# Run again without piping anything
			$CMD;
			exit 1
		fi

		# Test if the files created were the same
		for PREFIX in aligned not.aligned 
		do
			NAME="bfast.$PREFIX.file.$OUTPUT_ID.$SPACE.$PAIRED_END";
			echo "          Comparing $NAME*";

			diff -q $OUTPUT_DIR/$NAME* $SAVE_DIR/$NAME*;

			# Get return code
			if [ "$?" -ne "0" ]; then
				echo "          $NAME* did not match.";
				exit 1
			fi
		done
	done
done

# Test passed!
echo "      Local alignment complete.";
exit 0