#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "../blib/BLibDefinitions.h"
#include "../blib/BError.h"
#include "../blib/BLib.h"
#include "../blib/RGBinary.h"
#include "../blib/RGMatch.h" 
#include "../blib/RGMatches.h" 
#include "../blib/AlignEntries.h"
#include "../blib/AlignEntry.h" 
#include "Align.h"
#include "ScoringMatrix.h"
#include "Definitions.h"
#include "RunAligner.h"

/* TODO */
void RunAligner(RGBinary *rg,
		char *matchFileName,
		char *scoringMatrixFileName,
		int colorSpace,
		int startContig,
		int startPos,
		int endContig,
		int endPos,
		int offsetLength,
		int maxNumMatches,
		int pairedEnd,
		int binaryInput,
		int numThreads,
		int usePairedEndLength,
		int pairedEndLength,
		int forceMirroring,
		char *outputID,
		char *outputDir,
		char *tmpDir,
		int binaryOutput,
		int *totalAlignTime,
		int *totalFileHandlingTime)
{
	char *FnName = "RunAligner";
	FILE *outputFP=NULL;
	FILE *notAlignedFP=NULL;
	FILE *matchFP=NULL;
	char outputFileName[MAX_FILENAME_LENGTH]="\0";
	char notAlignedFileName[MAX_FILENAME_LENGTH]="\0";

	/* Check rg to make sure it is in NT Space */
	if(rg->space != NTSpace) {
		PrintError(FnName,
				"rg->space",
				"The reference genome must be in NT space",
				Exit,
				OutOfRange);
	}

	/* Adjust start and end based on reference genome */
	AdjustBounds(rg,
			&startContig,
			&startPos,
			&endContig,
			&endPos);

	/* Create output file name */
	sprintf(outputFileName, "%s%s.aligned.file.%s.%d.%s",
			outputDir,
			PROGRAM_NAME,
			outputID,
			colorSpace,
			BFAST_ALIGN_FILE_EXTENSION);
	/* Create not aligned file name */
	sprintf(notAlignedFileName, "%s%s.not.aligned.file.%s.%d.%s",
			outputDir,
			PROGRAM_NAME,
			outputID,
			colorSpace,
			BFAST_NOT_ALIGNED_FILE_EXTENSION);

	/* Open output file */
	if((outputFP=fopen(outputFileName, "wb"))==0) {
		PrintError(FnName,
				outputFileName,
				"Could not open file for writing",
				Exit,
				OpenFileError);
	}

	/* Open not aligned file */
	if((notAlignedFP=fopen(notAlignedFileName, "wb"))==0) {
		PrintError(FnName,
				notAlignedFileName,
				"Could not open file for writing",
				Exit,
				OpenFileError);
	}

	if(VERBOSE >= 0) {
		fprintf(stderr, "Will output aligned reads to %s.\n", outputFileName);
		fprintf(stderr, "Will output unaligned reads to %s.\n", notAlignedFileName);
		fprintf(stderr, "%s", BREAK_LINE);
	}

	if(VERBOSE >= 0) {
		fprintf(stderr, "%s", BREAK_LINE);
		fprintf(stderr, "Reading match file from %s.\n",
				matchFileName);
	}

	/* Open current match file */
	if((matchFP=fopen(matchFileName, "rb"))==0) {
		PrintError(FnName,
				matchFileName,
				"Could not open file for reading",
				Exit,
				OpenFileError);
	}

	RunDynamicProgramming(matchFP,
			rg,
			scoringMatrixFileName,
			colorSpace,
			startContig,
			startPos,
			endContig,
			endPos,
			offsetLength,
			maxNumMatches,
			pairedEnd,
			binaryInput,
			numThreads,
			usePairedEndLength,
			pairedEndLength,
			forceMirroring,
			tmpDir,
			outputFP,
			notAlignedFP,
			binaryOutput,
			totalAlignTime,
			totalFileHandlingTime);

	if(VERBOSE >= 0) {
		fprintf(stderr, "%s", BREAK_LINE);
	}

	/* Close the match file */
	fclose(matchFP);

	/* Close output file */
	fclose(outputFP);

	/* Close not aligned file */
	fclose(notAlignedFP);
}

/* TODO */
void RunDynamicProgramming(FILE *matchFP,
		RGBinary *rg,
		char *scoringMatrixFileName,
		int colorSpace,
		int startContig,
		int startPos,
		int endContig,
		int endPos,
		int offsetLength,
		int maxNumMatches,
		int pairedEnd,
		int binaryInput,
		int numThreads,
		int usePairedEndLength,
		int pairedEndLength,
		int forceMirroring,
		char *tmpDir,
		FILE *outputFP,
		FILE *notAlignedFP,
		int binaryOutput,
		int *totalAlignTime,
		int *totalFileHandlingTime)
{
	/* local variables */
	ScoringMatrix sm;
	RGMatches m;
	int i;
	int continueReading=0;
	int numMatches=0;
	int numAligned=0;
	int numNotAligned=0;
	int startTime, endTime;
	AlignEntries aEntries;
	/* Thread specific data */
	ThreadData *data;
	pthread_t *threads=NULL;
	int errCode;
	void *status;

	/* Initialize */
	RGMatchesInitialize(&m);
	ScoringMatrixInitialize(&sm);

	/* Allocate memory for thread arguments */
	data = malloc(sizeof(ThreadData)*numThreads);
	if(NULL==data) {
		PrintError("RunDynamicProgramming",
				"data",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	/* Allocate memory for thread ids */
	threads = malloc(sizeof(pthread_t)*numThreads);
	if(NULL==threads) {
		PrintError("RunDynamicProgramming",
				"threads",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Start file handling timer */
	startTime = time(NULL);

	/* Read in scoring matrix */
	ScoringMatrixRead(scoringMatrixFileName, &sm, colorSpace); 

	/**/
	/* Split the input file into equal temp files for each thread */
	/* Could put this in a separate function */
	/**/

	/* Open temp files for the threads */
	for(i=0;i<numThreads;i++) {
		data[i].inputFP = OpenTmpFile(tmpDir, &data[i].inputFileName);
		data[i].outputFP = OpenTmpFile(tmpDir, &data[i].outputFileName);
		data[i].notAlignedFP = OpenTmpFile(tmpDir, &data[i].notAlignedFileName); 
	}

	/* Go through each read in the match file and partition them for the threads */
	if(VERBOSE >= 0) {
		fprintf(stderr, "Filtering and partitioning matches for threads...\n0");
	}
	i=0;
	while(EOF!=RGMatchesRead(matchFP, 
				&m,
				pairedEnd,
				binaryInput) 
		 ) {

		if(VERBOSE >= 0 && numMatches%PARTITION_MATCHES_ROTATE_NUM==0) {
			fprintf(stderr, "\r[%d]", numMatches);
		}
		/* increment */
		numMatches++;

		/* Filter the matches that are not within the bounds */
		/* Filter one if it has too many entries */
		RGMatchesFilterOutOfRange(&m,
				startContig,
				startPos,
				endContig,
				endPos,
				maxNumMatches);

		/* Filter those reads we will not be able to align */
		/* line 1 - if both were found to have too many alignments in bmatches */
		/* line 3 - if both do not have any possible matches */
		if( (m.matchOne.maxReached == 1 && (pairedEnd == 0 || m.matchTwo.maxReached == 1)) ||
				(m.matchOne.numEntries <= 0 && (pairedEnd == 0 || m.matchTwo.numEntries <= 0))
		  )	{
			/* Print to the not aligned file */
			numNotAligned++;
			RGMatchesPrint(notAlignedFP,
					&m,
					binaryOutput);
		}
		else {
			/* Print match to temp file */
			RGMatchesPrint(data[i].inputFP,
					&m,
					binaryInput);
			/* Increment */
			i = (i+1)%numThreads;
		}

		/* Free memory */
		RGMatchesFree(&m);
	}
	if(VERBOSE >= 0) {
		fprintf(stderr, "\r[%d]\n", numMatches);
		fprintf(stderr, "Initially filtered %d out of %d reads.\n",
				numNotAligned,
				numMatches);
	}

	/* End file handling timer */
	endTime = time(NULL);
	(*totalFileHandlingTime) += endTime - startTime;

	/* Create thread arguments */
	for(i=0;i<numThreads;i++) {
		fseek(data[i].inputFP, 0, SEEK_SET);
		fseek(data[i].outputFP, 0, SEEK_SET);
		data[i].rg=rg;
		data[i].colorSpace=colorSpace;
		data[i].offsetLength=offsetLength;
		data[i].pairedEnd=pairedEnd;
		data[i].usePairedEndLength = usePairedEndLength;
		data[i].pairedEndLength = pairedEndLength;
		data[i].forceMirroring = forceMirroring;
		data[i].binaryInput=binaryInput;
		data[i].binaryOutput=binaryOutput;
		data[i].sm = &sm;
		data[i].threadID = i;
	}

	if(VERBOSE >= 0) {
		fprintf(stderr, "%s", BREAK_LINE);
		fprintf(stderr, "Processing %d reads.\n",
				numMatches - numNotAligned
				);
		fprintf(stderr, "%s", BREAK_LINE);
		fprintf(stderr, "Performing alignment...\n");
		fprintf(stderr, "Currently on:\n0");
	}

	/* Start align timer */
	startTime = time(NULL);

	/* Create threads */
	for(i=0;i<numThreads;i++) {
		/* Start thread */
		errCode = pthread_create(&threads[i], /* thread struct */
				NULL, /* default thread attributes */
				RunDynamicProgrammingThread, /* start routine */
				&data[i]); /* data to routine */
		if(0!=errCode) {
			PrintError("RunDynamicProgramming",
					"pthread_create: errCode",
					"Could not start thread",
					Exit,
					ThreadError);
		}
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Created threadID:%d\n",
					i);
		}
	}

	/* Wait for the threads to finish */
	for(i=0;i<numThreads;i++) {
		/* Wait for the given thread to return */
		errCode = pthread_join(threads[i],
				&status);
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Thread returned with errCode:%d\n",
					errCode);
		}
		/* Check the return code of the thread */
		if(0!=errCode) {
			PrintError("FindMatchesInIndexes",
					"pthread_join: errCode",
					"Thread returned an error",
					Exit,
					ThreadError);
		}
		/* Reinitialize file pointer */
		fseek(data[i].outputFP, 0, SEEK_SET);
		assert(NULL!=data[i].outputFP);
	}
	if(VERBOSE >= 0) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Alignment complete.\n");
	}

	/* End align timer */
	endTime = time(NULL);
	(*totalAlignTime) += endTime - startTime;

	/* Start file handling timer */
	startTime = time(NULL);

	/* Close tmp input files */
	for(i=0;i<numThreads;i++) {
		CloseTmpFile(&data[i].inputFP, &data[i].inputFileName);
	}

	/* Merge all the aligned reads from the threads */
	if(VERBOSE >=0) {
		fprintf(stderr, "Merging and outputting aligned reads from threads...\n[0]");
	}
	AlignEntriesInitialize(&aEntries);
	numAligned=0;
	continueReading=1;
	while(continueReading==1) {
		/* Get an align from a thread */
		continueReading=0;
		for(i=0;i<numThreads;i++) {
			/* Read in the align entries */
			if(EOF != AlignEntriesRead(&aEntries, data[i].outputFP, pairedEnd, colorSpace, binaryOutput)) {
				if(VERBOSE >=0 && numAligned%ALIGN_ROTATE_NUM == 0) {
					fprintf(stderr, "\r[%d]", numAligned);
				}
				continueReading=1;
				/* Update the number that were aligned */
				assert(aEntries.numEntriesOne > 0 ||
						(aEntries.pairedEnd == 1 && aEntries.numEntriesTwo > 0));
				numAligned++;
				/* Print it out */
				AlignEntriesPrint(&aEntries,
						outputFP,
						binaryOutput);
			}
			AlignEntriesFree(&aEntries);
		}
	}
	if(VERBOSE >=0) {
		fprintf(stderr, "\r[%d]\n", numAligned);
	}

	/* Merge the not aligned tmp files */
	if(VERBOSE >=0) {
		fprintf(stderr, "Merging and outputting unaligned reads from threads and initial filter...\n");
	}
	for(i=0;i<numThreads;i++) {
		fseek(data[i].notAlignedFP, 0, SEEK_SET);

		continueReading=1;
		while(continueReading==1) {
			RGMatchesInitialize(&m);
			if(RGMatchesRead(data[i].notAlignedFP,
						&m,
						pairedEnd,
						0) == EOF) {
				continueReading = 0;
			}
			else {
				if(VERBOSE >= 0 && numNotAligned%ALIGN_ROTATE_NUM==0) {
					fprintf(stderr, "\r[%d]", numNotAligned);
				}
				numNotAligned++;
				RGMatchesPrint(notAlignedFP,
						&m,
						binaryOutput);

				RGMatchesFree(&m);
			}
		}
	}
	if(VERBOSE >=0) {
		fprintf(stderr, "\r[%d]\n", numNotAligned);
	}

	/* Close tmp output files */
	for(i=0;i<numThreads;i++) {
		CloseTmpFile(&data[i].outputFP, &data[i].outputFileName);
		CloseTmpFile(&data[i].notAlignedFP, &data[i].notAlignedFileName);
	}

	/* Start file handling timer */
	endTime = time(NULL);
	(*totalFileHandlingTime) += endTime - startTime;

	if(VERBOSE >=0) {
		fprintf(stderr, "Outputted alignments for %d reads.\n", numAligned);
		fprintf(stderr, "Outputted %d reads for which there were no alignments.\n", numNotAligned); 
		fprintf(stderr, "Outputting complete.\n");
	}
	assert(numAligned + numNotAligned == numMatches);

	/* Free memory */
	free(data);
	free(threads);
	AlignEntriesFree(&aEntries);
	/* Free scores */
	ScoringMatrixFree(&sm);
}

/* TODO */
void *RunDynamicProgrammingThread(void *arg)
{
	/* Recover arguments */
	ThreadData *data = (ThreadData *)(arg);
	FILE *inputFP=data->inputFP;
	FILE *outputFP=data->outputFP;
	/*
	   FILE *notAlignedFP = data->notAlignedFP;
	   */
	RGBinary *rg=data->rg;
	int colorSpace=data->colorSpace;
	int offsetLength=data->offsetLength;
	int pairedEnd=data->pairedEnd;
	int usePairedEndLength=data->usePairedEndLength;
	int pairedEndLength=data->pairedEndLength;
	int forceMirroring=data->forceMirroring;
	int binaryInput=data->binaryInput;
	int binaryOutput=data->binaryOutput;
	int threadID=data->threadID;
	ScoringMatrix *sm = data->sm;
	/* Local variables */
	/*
	   char *FnName = "RunDynamicProgrammingThread";
	   */
	char matchRead[SEQUENCE_LENGTH]="\0";
	int matchReadLength=0;
	AlignEntries aEntries;
	int numAlignEntries=0;
	RGMatches m;
	int i;
	int numMatches=0;

	/* Initialize */
	RGMatchesInitialize(&m);
	AlignEntriesInitialize(&aEntries);

	/* Go through each read in the match file */
	while(EOF!=RGMatchesRead(inputFP, 
				&m,
				pairedEnd,
				binaryInput)) {
		numMatches++;
		numAlignEntries = 0;

		if(VERBOSE >= 0 && numMatches%ALIGN_ROTATE_NUM==0) {
			fprintf(stderr, "\rthread:%d\t[%d]", threadID, numMatches);
		}

		/* Check to see if we should try to align one read with no candidate
		 * locations if the other one has candidate locations.
		 *
		 * This assumes that the the first read is 5'->3' before the second read.
		 * */
		if(pairedEnd == 1 && usePairedEndLength == 1) {
			RGMatchesMirrorPairedEnd(&m, 
					pairedEndLength,
					forceMirroring);
		}

		/* Allocate memory for the AlignEntries */
		AlignEntriesAllocate(&aEntries,
				(char*)m.readName,
				m.matchOne.numEntries,
				m.matchTwo.numEntries,
				pairedEnd,
				colorSpace);

		/* Run the aligner */
		/* First entry */
		if(m.matchOne.numEntries > 0) {
			strcpy(matchRead, (char*)m.matchOne.read);
			matchReadLength = m.matchOne.readLength;
			if(colorSpace == 1) {
				matchReadLength = ConvertReadFromColorSpace(matchRead, matchReadLength);
			}
		}
		assert(m.matchOne.numEntries == aEntries.numEntriesOne);
		for(i=0;i<m.matchOne.numEntries;i++) { /* For each match */
			RunDynamicProgrammingThreadHelper(rg,
					m.matchOne.contigs[i],
					m.matchOne.positions[i],
					m.matchOne.strand[i],
					matchRead,
					matchReadLength,
					colorSpace,
					offsetLength,
					sm,
					&aEntries.entriesOne[i]);
		}
		/* Second entry */
		if(m.matchTwo.numEntries > 0) {
			strcpy(matchRead, (char*)m.matchTwo.read);
			matchReadLength = m.matchTwo.readLength;
			if(colorSpace == 1) {
				matchReadLength = ConvertReadFromColorSpace(matchRead, matchReadLength);
			}
		}
		assert(m.matchTwo.numEntries == aEntries.numEntriesTwo);
		for(i=0;i<m.matchTwo.numEntries;i++) { /* For each match */
			RunDynamicProgrammingThreadHelper(rg,
					m.matchTwo.contigs[i],
					m.matchTwo.positions[i],
					m.matchTwo.strand[i],
					matchRead,
					matchReadLength,
					colorSpace,
					offsetLength,
					sm,
					&aEntries.entriesTwo[i]);
		}

		if(aEntries.numEntriesOne > 0 ||
				(pairedEnd == 1 && aEntries.numEntriesTwo > 0)) {

			/* Remove duplicates */
			AlignEntriesRemoveDuplicates(&aEntries,
					AlignEntrySortByAll);
		}

		assert(pairedEnd == aEntries.pairedEnd);

		/* Output alignment */
		AlignEntriesPrint(&aEntries,
				outputFP,
				binaryOutput);

		/* Free memory */
		AlignEntriesFree(&aEntries);
		RGMatchesFree(&m);
	}
	if(VERBOSE >= 0) {
		fprintf(stderr, "\rthread:%d\t[%d]", threadID, numMatches);
	}


	return arg;
}

/* TODO */
void RunDynamicProgrammingThreadHelper(RGBinary *rg,
		uint32_t contig,
		uint32_t position,
		int8_t strand,
		char *read,
		int readLength,
		int colorSpace,
		int offsetLength,
		ScoringMatrix *sm,
		AlignEntry *aEntry)
{
	char *FnName = "RunDynamicProgrammingThreadHelper";
	char *reference=NULL;
	int referenceLength=0;
	int referencePosition=0;
	int adjustPosition=0;

	/* Get the appropriate reference read */
	RGBinaryGetReference(rg,
			contig,
			position,
			FORWARD, /* We have been just reversing the read instead of the reference */
			offsetLength,
			&reference,
			readLength,
			&referenceLength,
			&referencePosition);
	assert(referenceLength > 0);

	/* Get alignment */
	adjustPosition=Align(read,
			readLength,
			reference,
			referenceLength,
			sm,
			aEntry,
			strand,
			colorSpace);

	/* Update adjustPosition based on offsetLength */
	assert(adjustPosition >= 0 && adjustPosition <= referenceLength);

	/* Update contig, position, strand and sequence name*/
	aEntry->contig = contig;
	aEntry->position = referencePosition+adjustPosition; /* Adjust position */
	aEntry->strand = strand;
	/* Allocate memory and copy over to contig name */
	aEntry->contigNameLength = rg->contigs[aEntry->contig-1].contigNameLength;
	aEntry->contigName = malloc(sizeof(char)*(aEntry->contigNameLength+1));
	if(NULL==aEntry->contigName) {
		PrintError(FnName,
				"aEntry->contigName",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	strcpy(aEntry->contigName, rg->contigs[aEntry->contig-1].contigName);

	/* Check align entry */
	/*
	   AlignEntryCheckReference(&aEntry[i], rg);
	   */
	/* Free memory */
	free(reference);
	reference=NULL;
}
