#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* 
   Based on GNU/Linux and glibc or not, argp.h may or may not be available.
   If it is not, fall back to getopt. Also see #ifdefs in the ParseInput.h file. 
   */
#ifdef HAVE_ARGP_H
#include <argp.h>
#define OPTARG arg
#elif defined HAVE_UNISTD_H
#include <unistd.h>
#define OPTARG optarg
#else
#include "GetOpt.h"
#define OPTARG optarg
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* For u_int etc. */
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* For Mac OS X with resource.h */
#endif

#ifdef HAVE_RESOURCE_H
#include <sys/resource.h>
#endif

#include <time.h>
#include "../blib/BLibDefinitions.h"
#include "../blib/BError.h"
#include "../blib/RGBinary.h"
#include "../blib/BLib.h"
#include "Definitions.h"
#include "RunAligner.h"
#include "ParseInput.h"

const char *argp_program_version =
"balign "
PACKAGE_VERSION
"\n"
"Copyright 2008-2009.";

const char *argp_program_bug_address =
PACKAGE_BUGREPORT;

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC, OPTIONAL_GROUP_NAME}.
   */
enum { 
	DescInputFilesTitle, DescRGFileName, DescMatchFileName, DescScoringMatrixFileName, 
	DescAlgoTitle, DescAlignmentType, DescBestOnly, DescSpace, DescScoringType, DescStartContig, DescStartPos, DescEndContig, DescEndPos, DescOffsetLength, DescMaxNumMatches, DescPairedEnd, DescNumThreads,
	DescPairedEndOptionsTitle, DescPairedEndLength, DescMirroringType, DescForceMirroring, 
	DescOutputTitle, DescOutputID, DescOutputDir, DescTmpDir, DescTiming, 
	DescMiscTitle, DescHelp
};

/* 
   The prototype for argp_option comes fron argp.h. If argp.h
   absent, then ParseInput.h declares it 
   */
static struct argp_option options[] = {
	{0, 0, 0, 0, "=========== Input Files =============================================================", 1},
	{"rgFileName", 'r', "rgFileName", 0, "Specifies the file name of the reference genome file", 1},
	{"matchFileName", 'm', "matchFileName", 0, "Specifies the bfast matches file", 1},
	{"scoringMatrixFileName", 'x', "scoringMatrixFileName", 0, "Specifies the file name storing the scoring matrix", 1},
	/*
	   {"binaryInput", 'b', 0, OPTION_NO_USAGE, "Specifies that the input matches files will be in binary format", 1},
	   */
	{0, 0, 0, 0, "=========== Algorithm Options: (Unless specified, default value = 0) ================", 2},
	{"alignmentType", 'a', "alignmentType", 0, "0: Full alignment 1: mismatches only", 2},
	{"bestOnly", 'b', 0, OPTION_NO_USAGE, "If specified prompts to find only the best scoring alignment(s) across all matches."
		"\n\t\t\tOtherwise, a best scoring alignment is output for each match.", 2},
	{"space", 'A', "space", 0, "0: NT space 1: Color space", 2},
	{"scoringType", 'X', "scoringType", 0, "Final alignment score should by in 0: NT space 1: color space", 2},
	{"startContig", 's', "startContig", 0, "Specifies the start chromosome", 2},
	{"startPos", 'S', "startPos", 0, "Specifies the end position", 2},
	{"endContig", 'e', "endContig", 0, "Specifies the end chromosome", 2},
	{"endPos", 'E', "endPos", 0, "Specifies the end position", 2},
	{"offsetLength", 'O', "offset", 0, "Specifies the number of bases before and after the match to include in the reference genome", 2},
	{"maxNumMatches", 'M', "maxNumMatches", 0, "Specifies the maximum number of candidates to initiate alignment for a given match", 2},
	{"pairedEnd", '2', 0, OPTION_NO_USAGE, "Specifies that paired end data is to be expected", 2},
	{"numThreads", 'n', "numThreads", 0, "Specifies the number of threads to use (Default 1)", 2},
	{0, 0, 0, 0, "=========== Paired End Options ======================================================", 3},
	{"pairedEndLength", 'l', "pairedEndLength", 0, "Specifies that if one read of the pair has CALs and the other does not,"
		"\n\t\t\tthis distance will be used to infer the latter read's CALs", 3},
	{"mirroringType", 'L', "mirroringType", 0, "0: No mirroring should occur 1: specifies that we assume that the first end"
		"\n\t\t\tis before the second end (5'->3') 2: specifies that we assume that the first end is before the second"
			"\n\t\t\tend (5'->3') 3: specifies that we mirror CALs in both directions", 3},
	{"forceMirroring", 'f', 0, OPTION_NO_USAGE, "Specifies that we should always mirror CALs using the distance from -l", 3},
	{0, 0, 0, 0, "=========== Output Options ==========================================================", 4},
	{"outputID", 'o', "outputID", 0, "Specifies the name to identify the output files", 4},
	{"outputDir", 'd', "outputDir", 0, "Specifies the output directory for the output files", 4},
	{"tmpDir", 'T', "tmpDir", 0, "Specifies the directory in which to store temporary files", 4},
	/*
	   {"binaryOutput", 'B', 0, OPTION_NO_USAGE, "Specifies that the output aligned file will be in binary format", 4},
	   */
	{"timing", 't', 0, OPTION_NO_USAGE, "Specifies to output timing information", 4},
	{0, 0, 0, 0, "=========== Miscellaneous Options ===================================================", 5},
	{"Parameters", 'p', 0, OPTION_NO_USAGE, "Print program parameters", 5},
	{"Help", 'h', 0, OPTION_NO_USAGE, "Display usage summary", 5},
	{0, 0, 0, 0, 0, 0}
};

#ifdef HAVE_ARGP_H
/*
   ARGS_DOC. Field 3 in ARGP.
   A description of the non-option command-line arguments that we accept.
   Not complete yet. So empty string
   */
static char args_doc[] = "";
/*
   DOC.  Field 4 in ARGP.  Program documentation.
   */
static char doc[] = "";
/*
   The ARGP structure itself.
   */
static struct argp argp = {options, parse_opt, args_doc, doc};
#else
/* argp.h support not available! Fall back to getopt */
static char OptionString[]=
"a:d:e:l:m:n:o:r:s:x:A:E:H:L:M:O:S:T:X:2bfhpt";
#endif

enum {ExecuteGetOptHelp, ExecuteProgram, ExecutePrintProgramParameters};

/*
   The main function. All command-line options parsed using argp_parse
   or getopt whichever available
   */

	int
main (int argc, char **argv)
{
	struct arguments arguments;
	RGBinary rg;
	time_t startTotalTime = time(NULL);
	time_t endTotalTime;
	time_t startTime;
	time_t endTime;
	int totalReferenceGenomeTime = 0; /* Total time to load and delete the reference genome */
	int totalAlignTime = 0; /* Total time to align the reads */
	int totalFileHandlingTime = 0; /* Total time partitioning and merging matches and alignments respectively */
	int seconds, minutes, hours;
	if(argc>1) {
		/* Set argument defaults. (overriden if user specifies them)  */ 
		AssignDefaultValues(&arguments);

		/* Parse command line args */
#ifdef HAVE_ARGP_H
		if(argp_parse(&argp, argc, argv, 0, 0, &arguments)==0)
#else
			if(getopt_parse(argc, argv, OptionString, &arguments)==0)
#endif 
			{
				switch(arguments.programMode) {
					case ExecuteGetOptHelp:
						GetOptHelp();
						break;
					case ExecutePrintProgramParameters:
						PrintProgramParameters(stderr, &arguments);
						break;
					case ExecuteProgram:
						if(ValidateInputs(&arguments)) {
							fprintf(stderr, "**** Input arguments look good! *****\n");
							fprintf(stderr, BREAK_LINE);
						}
						else {
							PrintError("PrintError",
									NULL,
									"validating command-line inputs",
									Exit,
									InputArguments);

						}
						PrintProgramParameters(stderr, &arguments);
						/* Execute Program */
						startTime = time(NULL);
						RGBinaryReadBinary(&rg,
								arguments.rgFileName);
						/* Unpack */
						/*
						   RGBinaryUnPack(&rg);
						   */
						endTime = time(NULL);
						totalReferenceGenomeTime = endTime - startTime;
						/* Run the aligner */
						RunAligner(&rg,
								arguments.matchFileName,
								arguments.scoringMatrixFileName,
								arguments.alignmentType,
								arguments.bestOnly,
								arguments.space,
								arguments.scoringType,
								arguments.startContig,
								arguments.startPos,
								arguments.endContig,
								arguments.endPos,
								arguments.offsetLength,
								arguments.maxNumMatches,
								arguments.pairedEnd,
								arguments.binaryInput,
								arguments.numThreads,
								arguments.usePairedEndLength,
								arguments.pairedEndLength,
								arguments.mirroringType,
								arguments.forceMirroring,
								arguments.outputID,
								arguments.outputDir,
								arguments.tmpDir,
								arguments.binaryOutput,
								&totalAlignTime,
								&totalFileHandlingTime);
						/* Free the Reference Genome */
						RGBinaryDelete(&rg);

						if(arguments.timing == 1) {
							/* Output loading reference genome time */
							seconds = totalReferenceGenomeTime;
							hours = seconds/3600;
							seconds -= hours*3600;
							minutes = seconds/60;
							seconds -= minutes*60;
							fprintf(stderr, "Reference Genome loading time took: %d hours, %d minutes and %d seconds.\n",
									hours,
									minutes,
									seconds
								   );

							/* Output aligning time */
							seconds = totalAlignTime;
							hours = seconds/3600;
							seconds -= hours*3600;
							minutes = seconds/60;
							seconds -= minutes*60;
							fprintf(stderr, "Align time took: %d hours, %d minutes and %d seconds.\n",
									hours,
									minutes,
									seconds
								   );

							/* Output file handling time */
							seconds = totalFileHandlingTime;
							hours = seconds/3600;
							seconds -= hours*3600;
							minutes = seconds/60;
							seconds -= minutes*60;
							fprintf(stderr, "File handling time took: %d hours, %d minutes and %d seconds.\n",
									hours,
									minutes,
									seconds
								   );

							/* Output total time */
							endTotalTime = time(NULL);
							seconds = endTotalTime - startTotalTime;
							hours = seconds/3600;
							seconds -= hours*3600;
							minutes = seconds/60;
							seconds -= minutes*60;
							fprintf(stderr, "Total time elapsed: %d hours, %d minutes and %d seconds.\n",
									hours,
									minutes,
									seconds
								   );
						}
						fprintf(stderr, "Terminating successfully!\n");
						fprintf(stderr, "%s", BREAK_LINE);
						break;
					default:
						PrintError("PrintError",
								"programMode",
								"Could not determine program mode",
								Exit,
								OutOfRange);
				}
			}
			else {
				PrintError("PrintError",
						NULL,
						"Could not parse command line argumnets",
						Exit,
						InputArguments);
			}
		/* Free program parameters */
		FreeProgramParameters(&arguments);
	}
	else {
		GetOptHelp();
#ifdef HAVE_ARGP_H
		/* fprintf(stderr, "Type \"%s --help\" to see usage\n", argv[0]); */
#else
		/*     fprintf(stderr, "Type \"%s -h\" to see usage\n", argv[0]); */
#endif
	}
	return 0;
}

int ValidateInputs(struct arguments *args) {

	char *FnName="ValidateInputs";

	fprintf(stderr, BREAK_LINE);
	fprintf(stderr, "Checking input parameters supplied by the user ...\n");

	if(args->rgFileName!=0) {
		fprintf(stderr, "Validating rgFileName %s. \n",
				args->rgFileName);
		if(ValidateFileName(args->rgFileName)==0)
			PrintError(FnName, "rgFileName", "Command line argument", Exit, IllegalFileName);
	}

	if(args->matchFileName!=0) {
		fprintf(stderr, "Validating matchFileName path %s. \n",
				args->matchFileName);
		if(ValidateFileName(args->matchFileName)==0)
			PrintError(FnName, "matchFileName", "Command line argument", Exit, IllegalFileName);
	}

	if(args->scoringMatrixFileName!=0) {
		fprintf(stderr, "Validating scoringMatrixFileName path %s. \n",
				args->scoringMatrixFileName);
		if(ValidateFileName(args->scoringMatrixFileName)==0)
			PrintError(FnName, "scoringMatrixFileName", "Command line argument", Exit, IllegalFileName);
	}

	if(args->alignmentType != FullAlignment && args->alignmentType != MismatchesOnly) {
		PrintError(FnName, "alignmentType", "Command line argument", Exit, OutOfRange);
	}

	if(args->bestOnly != BestOnly && args->bestOnly != AllAlignments) {
		PrintError(FnName, "bestOnly", "Command line argument", Exit, OutOfRange);
	}

	if(args->space != NTSpace && args->space != ColorSpace) {
		PrintError(FnName, "space", "Command line argument", Exit, OutOfRange);
	}

	if(args->scoringType != NTSpace && args->scoringType != ColorSpace) {
		PrintError(FnName, "scoringType", "Command line argument", Exit, OutOfRange);
	}

	if(args->startContig < 0) {
		PrintError(FnName, "startContig", "Command line argument", Exit, OutOfRange);
	}

	if(args->startPos < 0) {
		PrintError(FnName, "startPos", "Command line argument", Exit, OutOfRange);
	}

	if(args->endContig < 0) {
		PrintError(FnName, "endContig", "Command line argument", Exit, OutOfRange);
	}

	if(args->endPos < 0) {
		PrintError(FnName, "endPos", "Command line argument", Exit, OutOfRange);
	}

	if(args->offsetLength < 0) {
		PrintError(FnName, "offsetLength", "Command line argument", Exit, OutOfRange);
	}

	if(args->maxNumMatches < 0) {
		PrintError(FnName, "maxNumMatches", "Command line argument", Exit, OutOfRange);
	}

	if(args->pairedEnd < 0 || args->pairedEnd > 1) {
		PrintError(FnName, "pairedEnd", "Command line argument", Exit, OutOfRange);
	}

	if(args->numThreads<=0) {
		PrintError(FnName, "numThreads", "Command line argument", Exit, OutOfRange);
	} 

	if(args->outputID!=0) {
		fprintf(stderr, "Validating outputID %s. \n",
				args->outputID);
		if(ValidateFileName(args->outputID)==0)
			PrintError(FnName, "outputID", "Command line argument", Exit, IllegalFileName);
	}

	if(args->outputDir!=0) {
		fprintf(stderr, "Validating outputDir path %s. \n",
				args->outputDir);
		if(ValidateFileName(args->outputDir)==0)
			PrintError(FnName, "outputDir", "Command line argument", Exit, IllegalFileName);
	}

	if(args->tmpDir!=0) {
		fprintf(stderr, "Validating tmpDir path %s. \n",
				args->tmpDir);
		if(ValidateFileName(args->tmpDir)==0)
			PrintError(FnName, "tmpDir", "Command line argument", Exit, IllegalFileName);
	}

	/* If this does not hold, we have done something wrong internally */
	assert(args->timing == 0 || args->timing == 1);
	assert(args->binaryInput == TextInput || args->binaryInput == BinaryInput);
	assert(args->binaryOutput == TextOutput || args->binaryOutput == BinaryOutput);
	assert(args->usePairedEndLength == 0 || args->usePairedEndLength == 1);
	assert(args->forceMirroring == 0 || args->forceMirroring == 1);
	assert(NoMirroring <= args->mirroringType && args->mirroringType <= MirrorBoth);
	if(args->mirroringType != NoMirroring && args->usePairedEndLength == 0) {
		PrintError(FnName, "pairedEndLength", "Must specify a paired end length when using mirroring", Exit, OutOfRange);
	}
	if(args->forceMirroring == 1 && args->usePairedEndLength == 0) {
		PrintError(FnName, "pairedEndLength", "Must specify a paired end length when using force mirroring", Exit, OutOfRange);
	}

	return 1;
}

	void 
AssignDefaultValues(struct arguments *args)
{
	/* Assign default values */

	args->programMode = ExecuteProgram;

	args->rgFileName =
		(char*)malloc(sizeof(DEFAULT_FILENAME));
	assert(args->rgFileName!=0);
	strcpy(args->rgFileName, DEFAULT_FILENAME);

	args->matchFileName =
		(char*)malloc(sizeof(DEFAULT_FILENAME));
	assert(args->matchFileName!=0);
	strcpy(args->matchFileName, DEFAULT_FILENAME);

	args->scoringMatrixFileName =
		(char*)malloc(sizeof(DEFAULT_FILENAME));
	assert(args->scoringMatrixFileName!=0);
	strcpy(args->scoringMatrixFileName, DEFAULT_FILENAME);

	args->binaryInput = BMATCHES_DEFAULT_OUTPUT;
	args->binaryOutput = BALIGN_DEFAULT_OUTPUT;

	args->alignmentType = FullAlignment;
	args->bestOnly = AllAlignments;
	args->space = NTSpace;
	args->scoringType = NTSpace;
	args->startContig=0;
	args->startPos=0;
	args->endContig=INT_MAX;
	args->endPos=INT_MAX;
	args->offsetLength=0;
	args->maxNumMatches=INT_MAX;
	args->pairedEnd = 0;
	args->numThreads = 1;
	args->usePairedEndLength = 0;
	args->pairedEndLength = 0;
	args->mirroringType = NoMirroring;
	args->forceMirroring = 0;

	args->outputID =
		(char*)malloc(sizeof(DEFAULT_OUTPUT_ID));
	assert(args->outputID!=0);
	strcpy(args->outputID, DEFAULT_OUTPUT_ID);

	args->outputDir =
		(char*)malloc(sizeof(DEFAULT_OUTPUT_DIR));
	assert(args->outputDir!=0);
	strcpy(args->outputDir, DEFAULT_OUTPUT_DIR);

	args->tmpDir =
		(char*)malloc(sizeof(DEFAULT_OUTPUT_DIR));
	assert(args->tmpDir!=0);
	strcpy(args->tmpDir, DEFAULT_OUTPUT_DIR);

	args->timing = 0;

	return;
}

	void 
PrintProgramParameters(FILE* fp, struct arguments *args)
{
	char programmode[3][64] = {"ExecuteGetOptHelp", "ExecuteProgram", "ExecutePrintProgramParameters"};
	char using[2][64] = {"Not using", "Using"};
	fprintf(fp, BREAK_LINE);
	fprintf(fp, "Printing Program Parameters:\n");
	fprintf(fp, "programMode:\t\t\t\t%d\t[%s]\n", args->programMode, programmode[args->programMode]);
	fprintf(fp, "rgFileName:\t\t\t\t%s\n", args->rgFileName);
	fprintf(fp, "matchFileName:\t\t\t\t%s\n", args->matchFileName);
	fprintf(fp, "scoringMatrixFileName:\t\t\t%s\n", args->scoringMatrixFileName);
	/*
	   fprintf(fp, "binaryInput:\t\t\t\t%d\n", args->binaryInput);
	   */
	fprintf(fp, "alignmentType:\t\t\t\t%d\n", args->alignmentType);
	fprintf(fp, "bestOnly:\t\t\t\t%d\n", args->bestOnly);
	fprintf(fp, "space:\t\t\t\t\t%d\n", args->space);
	fprintf(fp, "scoringType:\t\t\t\t%d\n", args->scoringType);
	fprintf(fp, "startContig:\t\t\t\t%d\n", args->startContig);
	fprintf(fp, "startPos:\t\t\t\t%d\n", args->startPos);
	fprintf(fp, "endContig:\t\t\t\t%d\n", args->endContig);
	fprintf(fp, "endPos:\t\t\t\t\t%d\n", args->endPos);
	fprintf(fp, "offsetLength:\t\t\t\t%d\n", args->offsetLength);
	fprintf(fp, "maxNumMatches:\t\t\t\t%d\n", args->maxNumMatches);
	fprintf(fp, "pairedEnd:\t\t\t\t%d\n", args->pairedEnd);
	fprintf(fp, "numThreads:\t\t\t\t%d\n", args->numThreads);
	fprintf(fp, "pairedEndLength:\t\t\t%d\t[%s]\n", args->pairedEndLength, using[args->usePairedEndLength]);
	fprintf(fp, "mirroringType:\t\t\t\t%d\n", args->mirroringType);
	fprintf(fp, "forceMirroring:\t\t\t\t%d\n", args->forceMirroring);
	fprintf(fp, "outputID:\t\t\t\t%s\n", args->outputID);
	fprintf(fp, "outputDir:\t\t\t\t%s\n", args->outputDir);
	fprintf(fp, "tmpDir:\t\t\t\t\t%s\n", args->tmpDir);
	/*
	   fprintf(fp, "binaryOutput:\t\t\t\t%d\n", args->binaryOutput);
	   */
	fprintf(fp, "timing:\t\t\t\t\t%d\n", args->timing);
	fprintf(fp, BREAK_LINE);
	return;

}

/* TODO */
void FreeProgramParameters(struct arguments *args)
{
	free(args->rgFileName);
	args->rgFileName=NULL;
	free(args->matchFileName);
	args->matchFileName=NULL;
	free(args->scoringMatrixFileName);
	args->scoringMatrixFileName=NULL;
	free(args->outputID);
	args->outputID=NULL;
	free(args->outputDir);
	args->outputDir=NULL;
	free(args->tmpDir);
	args->tmpDir=NULL;
}

void
GetOptHelp() {

	struct argp_option *a=options;
	fprintf(stderr, "%s\n", argp_program_version);
	fprintf(stderr, "\nUsage: balign [options]\n");
	while((*a).group>0) {
		switch((*a).key) {
			case 0:
				fprintf(stderr, "\n%s\n", (*a).doc); break;
			default:
				if((*a).arg != 0) {
					fprintf(stderr, "-%c\t%12s\t%s\n", (*a).key, (*a).arg, (*a).doc); 
				}
				else {
					fprintf(stderr, "-%c\t%12s\t%s\n", (*a).key, "", (*a).doc); 
				}
				break;
		}
		a++;
	}
	fprintf(stderr, "\n send bugs to %s\n", argp_program_bug_address);
	return;
}

#ifdef HAVE_ARGP_H
	static error_t
parse_opt (int key, char *arg, struct argp_state *state) 
{
	struct arguments *arguments = state->input;
#else
	int
		getopt_parse(int argc, char** argv, char OptionString[], struct arguments* arguments) 
		{
			char key;
			int OptErr=0;
			while((OptErr==0) && ((key = getopt (argc, argv, OptionString)) != -1)) {
				/*
				   fprintf(stderr, "Key is %c and OptErr = %d\n", key, OptErr);
				   */
#endif
				switch (key) {
					case '2':
						arguments->pairedEnd = 1;break;
						/*
						   case 'b':
						   arguments->binaryInput = 1;break;
						   */
					case 'a':
						arguments->alignmentType=atoi(OPTARG);break;
					case 'b':
						arguments->bestOnly=atoi(OPTARG);break;
					case 'd':
						StringCopyAndReallocate(&arguments->outputDir, OPTARG);
						/* set the tmp directory to the output director */
						if(strcmp(arguments->tmpDir, DEFAULT_FILENAME)==0) {
							StringCopyAndReallocate(&arguments->tmpDir, OPTARG);
						}
						break;
					case 'e':
						arguments->endContig=atoi(OPTARG);break;
					case 'f':
						arguments->forceMirroring=1;break;
					case 'h':
						arguments->programMode=ExecuteGetOptHelp; break;
					case 'l':
						arguments->usePairedEndLength=1;
						arguments->pairedEndLength = atoi(OPTARG);break;
					case 'm':
						StringCopyAndReallocate(&arguments->matchFileName, OPTARG);
						break;
					case 'n':
						arguments->numThreads=atoi(OPTARG); break;
					case 'o':
						StringCopyAndReallocate(&arguments->outputID, OPTARG);
						break;
					case 'p':
						arguments->programMode=ExecutePrintProgramParameters; break;
					case 'r':
						StringCopyAndReallocate(&arguments->rgFileName, OPTARG);
						break;
					case 's':
						arguments->startContig=atoi(OPTARG);break;
					case 't':
						arguments->timing = 1;break;
					case 'x':
						StringCopyAndReallocate(&arguments->scoringMatrixFileName, OPTARG);
						break;
					case 'A':
						arguments->space=atoi(OPTARG);break;
						/*
						   case 'B':
						   arguments->binaryOutput = 1;break;
						   */
					case 'E':
						arguments->endPos=atoi(OPTARG);break;
					case 'L':
						arguments->mirroringType=atoi(OPTARG);break;
					case 'M':
						arguments->maxNumMatches=atoi(OPTARG);break;
					case 'O':
						arguments->offsetLength=atoi(OPTARG);break;
					case 'S':
						arguments->startPos=atoi(OPTARG);break;
					case 'T':
						StringCopyAndReallocate(&arguments->tmpDir, OPTARG);
						break;
					case 'X':
						arguments->scoringType=atoi(OPTARG);break;
					default:
#ifdef HAVE_ARGP_H
						return ARGP_ERR_UNKNOWN;
				} /* switch */
				return 0;
#else
				OptErr=1;
			} /* while */
		} /* switch */
	return OptErr;
#endif
}
