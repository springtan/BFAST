#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "BLibDefinitions.h"
#include "BLib.h"
#include "BError.h"
#include "RGMatches.h"
#include "AlignedEntry.h"
#include "ScoringMatrix.h"
#include "Align.h"
#include "AlignMatrix.h"
#include "AlignNTSpace.h"


// DEBUGGING CODE NEEDS TO BE CLEANED UP

/* TODO */
int32_t AlignNTSpaceUngapped(char *read,
		char *mask,
		int readLength,
		char *reference,
		int referenceLength,
		int unconstrained,
		ScoringMatrix *sm,
		AlignedEntry *a,
		int offset,
		int32_t position,
		char strand)
{
	//char *FnName = "AlignNTSpaceUngapped";
	/* Read goes on the second row, reference on the first */
	int i, j;
	int32_t maxScore = NEGATIVE_INFINITY;
	int alignmentOffset=-1;
	int32_t curScore = 0.0;
	char curReference[SEQUENCE_LENGTH]="\0";
	char bestReference[SEQUENCE_LENGTH]="\0";

	assert(readLength <= referenceLength);
	assert(2*offset <= referenceLength); // should be exact, but this is ok

	/*
	char *FnName = "AlignNTSpaceUngapped";
	fprintf(stderr, "HERE in %s\n", FnName);
	fprintf(stderr, "offset=%d\n", offset);
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
			reference,
			reference + offset,
			read,
			mask);
			*/

	for(i=offset;i<referenceLength-readLength-offset+1;i++) { // Starting position 
		curScore = 0.0;
		for(j=0;j<readLength;j++) { // Position in the alignment
			if(Constrained == unconstrained &&
					'1' == mask[j] 
					&& ToLower(read[j]) != ToLower(reference[i+j])) { // they must match
				curScore = NEGATIVE_INFINITY;
				break;
			}
			curScore += ScoringMatrixGetNTScore(read[j], reference[i+j], sm);
			curReference[j] = reference[i+j];
		}
		curReference[j]='\0';
		if(maxScore < curScore) {
			maxScore = curScore;
			alignmentOffset = i;
			strcpy(bestReference, curReference);
		}
	}

	/* Copy over */
	if(NEGATIVE_INFINITY < maxScore) {
		AlignedEntryUpdateAlignment(a,
				(FORWARD==strand) ? (position + referenceLength - readLength - offset) : (position + offset),
				maxScore, 
				readLength, 
				readLength,
				read,
				bestReference,
				NULL);
		/*
		fprintf(stderr, "HERE updating alignent\n");
		AlignedEntryPrintText(a, stderr, NTSpace);
		fprintf(stderr, "%s\n", mask);
		*/
		return 1;
	}
	else {
		/* When can this happen you ask?  This can occur when the read
		 * has adaptor sequence at the beginning or end of the read. This
		 * causes it to look like there is an insertion at the beginning
		 * or end of the read.  This cannot be handled by ungapped local
		 * alignment.
		 * */
		assert(NULL == a->read);
		assert(NULL == a->reference);
		return 0;
	}
}

/* TODO */
void AlignNTSpaceGappedBounded(char *read,
		int readLength,
		char *reference,
		int referenceLength,
		ScoringMatrix *sm,
		AlignedEntry *a,
		AlignMatrix *matrix,
		int32_t position,
		char strand,
		int32_t maxH,
		int32_t maxV)
{
	//char *FnName = "AlignNTSpaceFullWithBound";
	/* read goes on the rows, reference on the columns */
	int i, j;

	assert(maxV >= 0 && maxH >= 0);
	assert(readLength < matrix->nrow);
	assert(referenceLength < matrix->ncol);

	AlignNTSpaceInitializeAtStart(matrix, sm, readLength, referenceLength);

	/* Fill in the matrix->cells according to the recursive rules */
	for(i=0;i<readLength;i++) { /* read/rows */
		for(j=GETMAX(0, i - maxV);
				j <= GETMIN(referenceLength-1, referenceLength - (readLength - maxH) + i);
				j++) { /* reference/columns */
			assert(i-maxV <= j && j <= referenceLength - (readLength - maxH) + i);

			// Fill in the cell
			AlignNTSpaceFillInCell(read, readLength, reference, referenceLength, sm, matrix, i+1, j+1, maxH, maxV);
		}
	}

	AlignNTSpaceRecoverAlignmentFromMatrix(a, matrix, read, readLength, reference, referenceLength, readLength - maxV, position, strand, 0);
}

void AlignNTSpaceGappedConstrained(char *read,
		char *mask,
		int readLength,
		char *reference,
		int referenceLength,
		ScoringMatrix *sm,
		AlignedEntry *a,
		AlignMatrix *matrix,
		int32_t referenceOffset,
		int32_t readOffset,
		int32_t position,
		char strand)
{
	char *FnName="AlignNTSpaceGappedConstrained";
	int32_t i, j;
	int32_t endRowStepOne, endColStepOne, endRowStepTwo, endColStepTwo;

	/* Get where to transition */
	endRowStepOne = endColStepOne = endRowStepTwo = endColStepTwo = -1;
	i=0;
	while(i<readLength) {
		if('1' == mask[i]) {
			endRowStepOne=i;
			endColStepOne=referenceOffset+i;
			break;
		}
		i++;
	}
	i=readLength;
	while(0<=i) {
		if('1' == mask[i]) {
			endRowStepTwo=i;
			endColStepTwo=referenceOffset+i;
			break;
		}
		i--;
	}

	/* Adjust based off of the read offset */
	if(0 < readOffset) {
		endRowStepOne += readOffset;
		endRowStepTwo += readOffset;
	}

	assert(0 <= endRowStepOne && 0 <= endColStepOne);
	assert(0 <= endRowStepTwo && 0 <= endColStepTwo);

	// DEBUGGING
	//fprintf(stderr, "%s", BREAK_LINE);
	//fprintf(stderr, "strand=%c\nposition=%d\n", strand, position);

	/* Step 1 - upper left */
	AlignNTSpaceInitializeAtStart(matrix, sm, endRowStepOne, endColStepOne);
	for(i=1;i<endRowStepOne+1;i++) { /* read/rows */ 
		for(j=1;j<endColStepOne+1;j++) { /* reference/columns */
			AlignNTSpaceFillInCell(read, readLength, reference, referenceLength, sm, matrix, i, j, INT_MAX, INT_MAX);
		}
	}

	// HERE
	/*
	   fprintf(stderr, "[%d,%d]\n", referenceOffset, readOffset);
	   fprintf(stderr, "%s\n%s\n%s\n",
	   reference,
	   read,
	   mask);
	   fprintf(stderr, "[%d,%d,%d,%d]\n",
	   endRowStepOne, endColStepOne,
	   endRowStepTwo, endColStepTwo);
	   */

	/* Step 2 - align along the mask */
	for(i=endRowStepOne,j=endColStepOne;
			i<endRowStepTwo && j<endColStepTwo;
			i++,j++) {
		if('1' == mask[i] && ToLower(read[i]) != ToLower(reference[j])) {
			PrintError(FnName, NULL, "read and reference did not match", Exit, OutOfRange);
		}
		/* Update diagonal */
		/* Get mismatch score */
		matrix->cells[i+1][j+1].s.score[0] = matrix->cells[i][j].s.score[0] + ScoringMatrixGetNTScore(read[i], reference[j], sm);
		matrix->cells[i+1][j+1].s.length[0] = matrix->cells[i][j].s.length[0] + 1;
		matrix->cells[i+1][j+1].s.from[0] = Match;
		/*
		   fprintf(stderr, "[i=%d,j=%d]\t%d,%d,%d\t%d+%d\n",
		   i, j,
		   matrix->cells[i][j].s.score[0],
		   matrix->cells[i][j].s.from[0],
		   matrix->cells[i][j].s.length[0],
		   matrix->cells[i][j].s.score[0],
		   ScoringMatrixGetNTScore(read[i], reference[j], sm));
		   */
	}
	assert(Match == matrix->cells[endRowStepTwo][endColStepTwo].s.from[0]);
	/*
	   fprintf(stderr, "[i=%d,j=%d]\t%d,%d,%d\t%d+%d\n",
	   i, j,
	   matrix->cells[i][j].s.score[0],
	   matrix->cells[i][j].s.from[0],
	   matrix->cells[i][j].s.length[0],
	   matrix->cells[i][j].s.score[0],
	   ScoringMatrixGetNTScore(read[i], reference[j], sm));
	   */

	// HERE
	/*
	   AlignedEntryInitialize(&tmp);
	   fprintf(stderr, "Step 2 alignment:\n");
	   AlignNTSpaceRecoverAlignmentFromMatrix(&tmp,
	   matrix,
	   read,
	   endRowStepTwo+1,
	   reference,
	   endColStepTwo+1,
	   endColStepTwo+1,
	   position,
	   strand,
	   0);
	   AlignedEntryPrintText(&tmp, stderr, NTSpace);
	   AlignedEntryFree(&tmp);
	   */

	/*
	   fprintf(stderr, "s(score,from,length)=[%d,%d,%d]\n",
	   matrix->cells[endRowStepOne][endColStepOne].s.score[0],
	   matrix->cells[endRowStepOne][endColStepOne].s.from[0],
	   matrix->cells[endRowStepOne][endColStepOne].s.length[0]);
	   assert(0 < matrix->cells[endRowStepOne][endColStepOne].s.length);
	   fprintf(stderr, "s(score,from,length)=[%d,%d,%d]\n",
	   matrix->cells[endRowStepTwo][endColStepTwo].s.score[0],
	   matrix->cells[endRowStepTwo][endColStepTwo].s.from[0],
	   matrix->cells[endRowStepTwo][endColStepTwo].s.length[0]);
	   assert(0 < matrix->cells[endRowStepTwo][endColStepTwo].s.length);
	   */

	/* Step 3 - lower right */
	AlignNTSpaceInitializeToExtend(matrix, sm, readLength, referenceLength, endRowStepTwo, endColStepTwo);
	// Note: we ignore any cells on row==endRowStepTwo or col==endRowStepTwo
	// since we assumed they were filled in by the previous re-initialization
	for(i=endRowStepTwo+1;i<readLength+1;i++) { /* read/rows */ 
		for(j=endColStepTwo+1;j<referenceLength+1;j++) { /* reference/columns */
			AlignNTSpaceFillInCell(read, readLength, reference, referenceLength, sm, matrix, i, j, INT_MAX, INT_MAX);
		}
	}

	/* Step 4 - recover alignment */
	AlignNTSpaceRecoverAlignmentFromMatrix(a, matrix, read, readLength, reference, referenceLength, endColStepTwo, position, strand, 0);

	// HERE
	/*
	fprintf(stderr, "Step 3 alignment:\n");
	AlignedEntryPrintText(a, stderr, NTSpace);
	*/
	//exit(1);
}

/* TODO */
void AlignNTSpaceRecoverAlignmentFromMatrix(AlignedEntry *a,
		AlignMatrix *matrix,
		char *read,
		int readLength,
		char *reference,
		int referenceLength,
		int toExclude,
		int32_t position,
		char strand,
		int debug)
{
	char *FnName="AlignNTSpaceRecoverAlignmentFromMatrix";
	int curRow, curCol, startRow, startCol;
	char curReadBase;
	int nextRow, nextCol;
	char nextReadBase;
	int curFrom;
	double maxScore;
	int32_t i, offset;
	char readAligned[SEQUENCE_LENGTH]="\0";
	char referenceAligned[SEQUENCE_LENGTH]="\0";
	int32_t referenceLengthAligned, length;

	curReadBase = nextReadBase = 'X';
	nextRow = nextCol = -1;

	assert(0 <= toExclude);

	/* Get the best alignment.  We can find the best score in the last row and then
	 * trace back.  We choose the best score from the last row since we want to 
	 * align the read completely and only locally to the reference. */
	startRow=-1;
	startCol=-1;
	maxScore = NEGATIVE_INFINITY;
	for(i=toExclude;i<referenceLength+1;i++) {
		/*
		   fprintf(stderr, "Checking [%d,%d] [%d,%d,%d]\n", readLength, i,
		   matrix->cells[readLength][i].s.score[0],
		   matrix->cells[readLength][i].s.from[0],
		   matrix->cells[readLength][i].s.length[0]
		   );
		   */
		assert(StartNT != matrix->cells[readLength][i].s.from[0]);
		/* Check only the first cell */
		if(maxScore < matrix->cells[readLength][i].s.score[0]) {
			maxScore = matrix->cells[readLength][i].s.score[0];
			startRow = readLength;
			startCol = i;
		}
	}
	assert(startRow >= 0 && startCol >= 0);
	assert(StartNT != matrix->cells[startRow][startCol].s.from[0]);

	// HERE
	/*
	   fprintf(stderr, "start(%d,%d,%d)\n",
	   matrix->cells[startRow][startCol].s.score[0],
	   matrix->cells[startRow][startCol].s.from[0],
	   matrix->cells[startRow][startCol].s.length[0]);

	   fprintf(stderr, "starting at %d,%d\n",
	   startRow,
	   startCol);
	   */

	/* Initialize variables for the loop */
	curRow=startRow;
	curCol=startCol;
	curFrom = Match;

	referenceLengthAligned=0;
	i=matrix->cells[curRow][curCol].s.length[0]-1; /* Get the length of the alignment */
	length=matrix->cells[curRow][curCol].s.length[0]; /* Copy over the length */

	/*
	   fprintf(stderr, "i=%d\tlength=%d\n", i, length);
	   */

	/* Now trace back the alignment using the "from" member in the matrix */
	while(0 <= i) {
		assert(0 <= curRow && 0 <= curCol);

		// HERE
		//fprintf(stderr, "%d,%d,%d,%d\n", curRow, curCol, i, curFrom);

		/* Where did the current cell come from */
		switch(curFrom) {
			case DeletionStart:
				curFrom = matrix->cells[curRow][curCol].s.from[0];
				assert(curFrom == Match || curFrom == InsertionExtension);
				break;
			case DeletionExtension:
				curFrom = matrix->cells[curRow][curCol].h.from[0];
				if(!(curFrom == DeletionStart || curFrom == DeletionExtension)) {
					fprintf(stderr, "\ncurFrom=%d\n",
							curFrom);
					fprintf(stderr, "toExclude=%d\n",
							toExclude);
					fprintf(stderr, "readLength=%d\n",
							readLength);
					fprintf(stderr, "startRow=%d\nstartCol=%d\n",
							startRow,
							startCol);
					fprintf(stderr, "curRow=%d\ncurCol=%d\n",
							curRow,
							curCol);
				}
				assert(curFrom == DeletionStart || curFrom == DeletionExtension);
				break;
			case Match:
				/*
				   fprintf(stderr, "LENGTH=%d\n",
				   matrix->cells[curRow][curCol].s.length[0]);
				   */
				curFrom = matrix->cells[curRow][curCol].s.from[0];
				break;
			case InsertionStart:
				curFrom = matrix->cells[curRow][curCol].s.from[0];
				if(!(curFrom == Match || curFrom == DeletionExtension)) {
					fprintf(stderr, "\ncurFrom=%d\n",
							curFrom);
					fprintf(stderr, "toExclude=%d\n",
							toExclude);
					fprintf(stderr, "readLength=%d\n",
							readLength);
					fprintf(stderr, "startRow=%d\nstartCol=%d\n",
							startRow,
							startCol);
					fprintf(stderr, "curRow=%d\ncurCol=%d\n",
							curRow,
							curCol);
				}
				assert(curFrom == Match || curFrom == DeletionExtension);
				break;
			case InsertionExtension:
				curFrom = matrix->cells[curRow][curCol].v.from[0];
				if(!(curFrom == InsertionStart || curFrom == InsertionExtension)) {
					fprintf(stderr, "\ncurFrom=%d\n",
							curFrom);
					fprintf(stderr, "toExclude=%d\n",
							toExclude);
					fprintf(stderr, "readLength=%d\n",
							readLength);
					fprintf(stderr, "startRow=%d\nstartCol=%d\n",
							startRow,
							startCol);
					fprintf(stderr, "curRow=%d\ncurCol=%d\n",
							curRow,
							curCol);
				}
				assert(curFrom == InsertionStart || curFrom == InsertionExtension);
				break;
			default:
				PrintError(FnName, "curFrom", "Could not recognize curFrom", Exit, OutOfRange);
		}

		assert(i>=0);

		/* Update alignment */
		switch(curFrom) {
			case DeletionStart:
			case DeletionExtension:
				readAligned[i] = GAP;
				referenceAligned[i] = reference[curCol-1];
				referenceLengthAligned++;
				nextRow = curRow;
				nextCol = curCol-1;
				break;
			case Match:
				readAligned[i] = read[curRow-1];
				referenceAligned[i] = reference[curCol-1];
				referenceLengthAligned++;
				nextRow = curRow-1;
				nextCol = curCol-1;
				break;
			case InsertionStart:
			case InsertionExtension:
				readAligned[i] = read[curRow-1];
				referenceAligned[i] = GAP;
				nextRow = curRow-1;
				nextCol = curCol;
				break;
			default:
				fprintf(stderr, "curFrom=%d\n", curFrom);
				PrintError(FnName, "curFrom", "Could not understand curFrom", Exit, OutOfRange);
		}

		// HERE
		/*
		   fprintf(stderr, "[%c,%c,%d,%d,%d,%d]\n",
		   referenceAligned[i],
		   readAligned[i],
		   i,
		   curFrom,
		   curRow,
		   curCol);
		   */

		assert(readAligned[i] != GAP || readAligned[i] != referenceAligned[i]);

		/* Update for next loop iteration */
		curRow = nextRow;
		curCol = nextCol;
		i--;

	} /* End Loop */
	if(-1 != i) {
		fprintf(stderr, "Weird: i=%d\n", i);
		fprintf(stderr, "CurRow = %d\tCurCol = %d\n",
				curRow, curCol);
		fprintf(stderr, "length=%d\n", length);
		readAligned[length]='\0';
		referenceAligned[length]='\0';
		int j;
		for(j=0;j<=i;j++) {
			readAligned[j] = referenceAligned[j] = 'X';
		}
		fprintf(stderr, "%s\n%s\n",
				readAligned,
				referenceAligned);
	}
	assert(-1==i);
	assert(referenceLengthAligned <= length);
	/*
	fprintf(stderr, "readLength=%d\tlength=%d\n", readLength, length); // HERE
	*/
	assert(readLength <= length);

	readAligned[length]='\0';
	referenceAligned[length]='\0';

	offset = curCol;

	/* Copy over */
	AlignedEntryUpdateAlignment(a,
			(FORWARD==strand) ? (position + offset) : (position + referenceLength - referenceLengthAligned - offset),
			maxScore, 
			referenceLengthAligned,
			length,
			readAligned,
			referenceAligned,
			NULL);
}

/* endRow and endCol should be the last row and column in the matrix 
 * you want to initialize.
 * */
void AlignNTSpaceInitializeAtStart(AlignMatrix *matrix,
		ScoringMatrix *sm,
		int32_t endRow,
		int32_t endCol)
{
	int32_t i, j;

	// Normal initialization */
	/* Allow the alignment to start anywhere in the reference */
	for(j=0;j<endCol+1;j++) {
		// Allow to start from a match
		matrix->cells[0][j].s.score[0] = 0;
		// Do not allow to start from an insertion or deletion
		matrix->cells[0][j].h.score[0] = matrix->cells[0][j].v.score[0] = NEGATIVE_INFINITY;
		matrix->cells[0][j].h.from[0] = matrix->cells[0][j].s.from[0] = matrix->cells[0][j].v.from[0] = StartNT;
		matrix->cells[0][j].h.length[0] = matrix->cells[0][j].s.length[0] = matrix->cells[0][j].v.length[0] = 0;
	}
	/* Align the full read */
	for(i=1;i<endRow+1;i++) {
		// Allow an insertion
		if(i == 1) { // Allow for an insertion start
			assert(0 == matrix->cells[i-1][0].s.length[0]);
			matrix->cells[i][0].v.score[0] = matrix->cells[i][0].s.score[0] = matrix->cells[i-1][0].s.score[0] + sm->gapOpenPenalty;
			matrix->cells[i][0].v.length[0] = matrix->cells[i][0].s.length[0] = matrix->cells[i-1][0].s.length[0] + 1;
			matrix->cells[i][0].v.from[0] = matrix->cells[i][0].s.from[0] = InsertionStart;
		}
		else { // Allow for an insertion extension
			assert(0 < matrix->cells[i-1][0].v.length[0]);
			matrix->cells[i][0].v.score[0] = matrix->cells[i][0].s.score[0] = matrix->cells[i-1][0].s.score[0] + sm->gapExtensionPenalty;
			matrix->cells[i][0].v.length[0] = matrix->cells[i][0].s.length[0] = matrix->cells[i-1][0].s.length[0] + 1;
			matrix->cells[i][0].v.from[0] = matrix->cells[i][0].s.from[0] = InsertionExtension;
		}
		// Do not allow a deletion
		matrix->cells[i][0].h.score[0] = NEGATIVE_INFINITY;
		matrix->cells[i][0].h.from[0] = StartNT;
		matrix->cells[i][0].h.length[0] = 0;
	}
}

/* Assumes that the "s" sub-cell at (startRow, startCol) in
 * the matrix has been initalized */
void AlignNTSpaceInitializeToExtend(AlignMatrix *matrix,
		ScoringMatrix *sm,
		int32_t readLength,
		int32_t referenceLength,
		int32_t startRow,
		int32_t startCol)
{
	int32_t i, j, endRow, endCol;

	endRow = readLength;
	endCol = referenceLength;

	// HERE
	/*
	fprintf(stderr, "AlignNTSpaceInitializeToExtend = [%d,%d]\n",
			startRow,
			startCol);
	*/

	// Special initialization 

	/* Initialize the corner cell */
	// Check that the match has been filled in 
	assert(Match == matrix->cells[startRow][startCol].s.from[0]); 
	assert(startRow <= matrix->cells[startRow][startCol].s.length[0]);
	// Do not allow a deletion or insertion
	matrix->cells[startRow][startCol].h.score[0] = matrix->cells[startRow][startCol].v.score[0] = NEGATIVE_INFINITY;
	matrix->cells[startRow][startCol].h.from[0] = matrix->cells[startRow][startCol].v.from[0] = StartNT;
	matrix->cells[startRow][startCol].h.length[0] = matrix->cells[startRow][startCol].v.length[0] = 0;

	for(j=startCol+1;j<endCol+1;j++) {  // Columns
		if(j == startCol + 1) { // Allow for a deletion start
			/*
			fprintf(stderr, "AlignNTSpaceInitializeToExtend [%d,%d] h.[%d,%d,%d]\n",
					startRow, j,
					matrix->cells[startRow][j-1].s.score[0],
					matrix->cells[startRow][j-1].s.length[0],
					matrix->cells[startRow][j-1].s.from[0]);
					*/

			matrix->cells[startRow][j].h.score[0] = matrix->cells[startRow][j].s.score[0] = matrix->cells[startRow][j-1].s.score[0] + sm->gapOpenPenalty;
			matrix->cells[startRow][j].h.length[0] = matrix->cells[startRow][j].s.length[0] = matrix->cells[startRow][j-1].s.length[0] + 1;
			matrix->cells[startRow][j].h.from[0] = matrix->cells[startRow][j].s.from[0] = DeletionStart;
		}
		else { // Allow for a deletion extension
			/* Deletion extension */
			assert(DeletionExtension == matrix->cells[startRow][j-1].h.from[0] ||
					DeletionStart == matrix->cells[startRow][j-1].h.from[0]); // We can constrain this more...
			matrix->cells[startRow][j].h.score[0] = matrix->cells[startRow][j].s.score[0] = matrix->cells[startRow][j-1].h.score[0] + sm->gapExtensionPenalty; 
			matrix->cells[startRow][j].h.length[0] = matrix->cells[startRow][j].s.length[0] = matrix->cells[startRow][j-1].h.length[0] + 1;
			matrix->cells[startRow][j].h.from[0] = matrix->cells[startRow][j].s.from[0] = DeletionExtension;
		}

		// Do not allow an insertion 
		matrix->cells[startRow][j].v.score[0] = NEGATIVE_INFINITY;
		matrix->cells[startRow][j].v.from[0] = StartNT;
		matrix->cells[startRow][j].v.length[0] = 0;
	}
	/* Align the full read */
	for(i=startRow+1;i<endRow+1;i++) {
		// Allow an insertion
		if(i == startRow + 1) { // Allow for an insertion start
			matrix->cells[i][startCol].v.score[0] = matrix->cells[i][startCol].s.score[0] = matrix->cells[i-1][startCol].s.score[0] + sm->gapOpenPenalty;
			matrix->cells[i][startCol].v.length[0] = matrix->cells[i][startCol].s.length[0] = matrix->cells[i-1][startCol].s.length[0] + 1;
			matrix->cells[i][startCol].v.from[0] = matrix->cells[i][startCol].s.from[0] = InsertionStart;
		}
		else { // Allow for an insertion extension
			assert(InsertionExtension == matrix->cells[i-1][startCol].v.from[0] ||
					InsertionStart == matrix->cells[i-1][startCol].v.from[0]);
			matrix->cells[i][startCol].v.score[0] = matrix->cells[i][startCol].s.score[0] = matrix->cells[i-1][startCol].v.score[0] + sm->gapExtensionPenalty; 
			matrix->cells[i][startCol].v.length[0] = matrix->cells[i][startCol].s.length[0] = matrix->cells[i-1][startCol].v.length[0] + 1;
			matrix->cells[i][startCol].v.from[0] = matrix->cells[i][startCol].s.from[0] = InsertionExtension;
		}

		// Do not allow a deletion
		matrix->cells[i][startCol].h.score[0] = NEGATIVE_INFINITY;
		matrix->cells[i][startCol].h.from[0] = StartNT;
		matrix->cells[i][startCol].h.length[0] = 0;
	}
}

void AlignNTSpaceFillInCell(char *read,
		int32_t readLength,
		char *reference,
		int32_t referenceLength,
		ScoringMatrix *sm,
		AlignMatrix *matrix,
		int32_t row,
		int32_t col,
		int32_t maxH,
		int32_t maxV) 
{
	/* Deletion relative to reference across a column */
	/* Insertion relative to reference is down a row */
	/* Match/Mismatch is a diagonal */

	assert(0 < row);
	assert(0 < col);

	/* Update deletion */
	if(maxV <= row - col) { // Out of bounds, do not consider
		matrix->cells[row][col].h.score[0] = NEGATIVE_INFINITY;
		matrix->cells[row][col].h.length[0] = INT_MIN;
		matrix->cells[row][col].h.from[0] = NoFromNT;
	}
	else {
		/* Deletion extension */
		matrix->cells[row][col].h.score[0] = matrix->cells[row][col-1].h.score[0] + sm->gapExtensionPenalty; 
		matrix->cells[row][col].h.length[0] = matrix->cells[row][col-1].h.length[0] + 1;
		matrix->cells[row][col].h.from[0] = DeletionExtension;
		/* Check if starting a new deletion is better */
		if(matrix->cells[row][col].h.score[0] < matrix->cells[row][col-1].s.score[0] + sm->gapOpenPenalty) {
			matrix->cells[row][col].h.score[0] = matrix->cells[row][col-1].s.score[0] + sm->gapOpenPenalty;
			matrix->cells[row][col].h.length[0] = matrix->cells[row][col-1].s.length[0] + 1;
			matrix->cells[row][col].h.from[0] = DeletionStart;
		}
	}

	/* Update insertion */
	if(maxH <= col - referenceLength + readLength - row) { // Out of bounds do not consider
		matrix->cells[row][col].v.score[0] = NEGATIVE_INFINITY;
		matrix->cells[row][col].v.length[0] = INT_MIN;
		matrix->cells[row][col].v.from[0] = NoFromNT;
	}
	else {
		/* Insertion extension */
		matrix->cells[row][col].v.score[0] = matrix->cells[row-1][col].v.score[0] + sm->gapExtensionPenalty; 
		matrix->cells[row][col].v.length[0] = matrix->cells[row-1][col].v.length[0] + 1;
		matrix->cells[row][col].v.from[0] = InsertionExtension;
		/* Check if starting a new insertion is better */
		if(matrix->cells[row][col].v.score[0] < matrix->cells[row-1][col].s.score[0] + sm->gapOpenPenalty) {
			matrix->cells[row][col].v.score[0] = matrix->cells[row-1][col].s.score[0] + sm->gapOpenPenalty;
			matrix->cells[row][col].v.length[0] = matrix->cells[row-1][col].s.length[0] + 1;
			matrix->cells[row][col].v.from[0] = InsertionStart;
		}
	}

	/* Update diagonal */
	/* Get mismatch score */
	matrix->cells[row][col].s.score[0] = matrix->cells[row-1][col-1].s.score[0] + ScoringMatrixGetNTScore(read[row-1], reference[col-1], sm);
	matrix->cells[row][col].s.length[0] = matrix->cells[row-1][col-1].s.length[0] + 1;
	matrix->cells[row][col].s.from[0] = Match;
	/* Get the maximum score of the three cases: horizontal, vertical and diagonal */
	if(matrix->cells[row][col].s.score[0] < matrix->cells[row][col].h.score[0]) {
		matrix->cells[row][col].s.score[0] = matrix->cells[row][col].h.score[0];
		matrix->cells[row][col].s.length[0] = matrix->cells[row][col].h.length[0];
		matrix->cells[row][col].s.from[0] = matrix->cells[row][col].h.from[0];
	}
	if(matrix->cells[row][col].s.score[0] < matrix->cells[row][col].v.score[0]) {
		matrix->cells[row][col].s.score[0] = matrix->cells[row][col].v.score[0];
		matrix->cells[row][col].s.length[0] = matrix->cells[row][col].v.length[0];
		matrix->cells[row][col].s.from[0] = matrix->cells[row][col].v.from[0];
	}
}
