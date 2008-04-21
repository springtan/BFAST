#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "BError.h"
#include "AlignEntry.h"

/* TODO */
void AlignEntryPrint(AlignEntry *aEntry,
		FILE *outputFP)
{
	/* Print the read name, alignment length, chromosome, position, strand, score */
	fprintf(outputFP, "%s\t%d\t%d\t%d\t%d\t%c\t%lf\n",
			aEntry->readName,
			aEntry->length,
			aEntry->referenceLength,
			aEntry->chromosome,
			aEntry->position,
			aEntry->strand,
			aEntry->score);

	/* Print the reference and read alignment */
	fprintf(outputFP, "%s\n%s\n",
			aEntry->reference,
			aEntry->read);
}

/* TODO */
int AlignEntryRead(AlignEntry *aEntry,
		FILE *inputFP)
{
	/* Read the read name, alignment length, chromosome, position, strand, score */
	if(fscanf(inputFP, "%s %d %d %d %d %c %lf\n",
				aEntry->readName,
				&aEntry->length,
				&aEntry->referenceLength,
				&aEntry->chromosome,
				&aEntry->position,
				&aEntry->strand,
				&aEntry->score)==EOF) {
		return EOF;
	}

	/* Read the reference and read alignment */
	if(fscanf(inputFP, "%s", aEntry->reference)==EOF) {
		PrintError("AlignEntryRead", 
				NULL, 
				"Could not read reference alignent",
				Exit,
				EndOfFile);
	}
	if(fscanf(inputFP, "%s", aEntry->read)==EOF) {
		PrintError("AlignEntryRead", 
				NULL, 
				"Could not read 'read' alignent",
				Exit,
				EndOfFile);
	}

	return 1;
}

/* TODO */
int AlignEntryRemoveDuplicates(AlignEntry **a,
		int length,
		int sortOrder)
{
	int i, prevIndex;

	if(length > 0) {
		/* Sort the array */
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Sorting... %d\n", length);
		}
		AlignEntryQuickSort(a, 0, length-1, sortOrder, 0, NULL, 0);
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Sorted\n");
		}

		/* Remove duplicates */
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Removing duplicates\n");
		}
		prevIndex=0;
		for(i=1;i<length;i++) {
			if(AlignEntryCompareAtIndex((*a), prevIndex, (*a), i, sortOrder)==0) {
				/* Free memory */
				assert((*a)[i].read!=NULL);
				free((*a)[i].read);
				assert((*a)[i].reference!=NULL);
				free((*a)[i].reference);
			}
			else {
				/* Increment prevIndex */
				prevIndex++;
				/* Copy to prevIndex (incremented) */
				AlignEntryCopyAtIndex((*a), i, (*a), prevIndex);
			}
		}

		/* Reallocate pair */
		length = prevIndex+1;
		(*a) = realloc((*a), sizeof(AlignEntry)*length);
		if(NULL == (*a)) {
			PrintError("AlignEntryRemoveDuplicates",
					"(*a)",
					"Could not reallocate Align Entries while removing duplicates",
					Exit,
					ReallocMemory);
		}

		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "Duplicates removed\n");
		}
	}
	return length;
}

/* TODO */
void AlignEntryQuickSort(AlignEntry **a,
		int low,
		int high,
		int sortOrder,
		int showPercentComplete,
		double *curPercent,
		int total)
{
	int i;
	int pivot=-1;
	AlignEntry *temp;

	if(low < high) {
		/* Allocate memory for the temp used for swapping */
		temp=malloc(sizeof(AlignEntry));
		if(NULL == temp) {
			PrintError("AlignEntryQuickSort",
					"temp",
					"Could not allocate temp",
					Exit,
					MallocMemory);
		}

		pivot = (low+high)/2;
		if(showPercentComplete == 1 && VERBOSE >= 0) {
			assert(NULL!=curPercent);
			if((*curPercent) < 100.0*((double)low)/total) {
				while((*curPercent) < 100.0*((double)low)/total) {
					(*curPercent) += SORT_ROTATE_INC;
				}
				fprintf(stderr, "\r%3.2lf percent complete", 100.0*((double)low)/total);
			}
		}

		AlignEntryCopyAtIndex((*a), pivot, temp, 0);
		AlignEntryCopyAtIndex((*a), high, (*a), pivot);
		AlignEntryCopyAtIndex(temp, 0, (*a), high);

		pivot = low;

		for(i=low;i<high;i++) {
			if(AlignEntryCompareAtIndex((*a), i, (*a), high, sortOrder) <= 0) {
				if(i!=pivot) {
					AlignEntryCopyAtIndex((*a), i, temp, 0);
					AlignEntryCopyAtIndex((*a), pivot, (*a), i);
					AlignEntryCopyAtIndex(temp, 0, (*a), pivot);
				}
				pivot++;
			}
		}
		AlignEntryCopyAtIndex((*a), pivot, temp, 0);
		AlignEntryCopyAtIndex((*a), high, (*a), pivot);
		AlignEntryCopyAtIndex(temp, 0, (*a), high);

		/* Free temp before the recursive call, otherwise we have a worst
		 * case of O(n) space (NOT IN PLACE) 
		 * */
		free(temp);

		AlignEntryQuickSort(a, low, pivot-1, sortOrder, showPercentComplete, curPercent, total);
		if(showPercentComplete == 1 && VERBOSE >= 0) {
			assert(NULL!=curPercent);
			if((*curPercent) < 100.0*((double)pivot)/total) {
				while((*curPercent) < 100.0*((double)pivot)/total) {
					(*curPercent) += SORT_ROTATE_INC;
				}
				fprintf(stderr, "\r%3.2lf percent complete", 100.0*((double)pivot)/total);
			}
		}
		AlignEntryQuickSort(a, pivot+1, high, sortOrder, showPercentComplete, curPercent, total);
		if(showPercentComplete == 1 && VERBOSE >= 0) {
			assert(NULL!=curPercent);
			if((*curPercent) < 100.0*((double)high)/total) {
				while((*curPercent) < 100.0*((double)high)/total) {
					(*curPercent) += SORT_ROTATE_INC;
				}
				fprintf(stderr, "\r%3.2lf percent complete", 100.0*((double)high)/total);
			}
		}
	}
}

/* TODO */
void AlignEntryCopyAtIndex(AlignEntry *src, int srcIndex, AlignEntry *dest, int destIndex)
{
	strcpy(dest[destIndex].readName, src[srcIndex].readName);
	dest[destIndex].read = src[srcIndex].read;
	dest[destIndex].reference = src[srcIndex].reference;
	dest[destIndex].length = src[srcIndex].length;
	dest[destIndex].referenceLength = src[srcIndex].referenceLength;
	dest[destIndex].chromosome = src[srcIndex].chromosome;
	dest[destIndex].position = src[srcIndex].position;
	dest[destIndex].strand = src[srcIndex].strand;
	dest[destIndex].score = src[srcIndex].score;

}

/* TODO */
int AlignEntryCompareAtIndex(AlignEntry *a, int indexA, AlignEntry *b, int indexB, int sortOrder)
{
	int cmp[6];
	int result;
	int i;

	if(sortOrder == AlignEntrySortByAll) {

		cmp[0] = strcmp(a[indexA].readName, b[indexB].readName);
		cmp[1] = strcmp(a[indexA].read, b[indexB].read);
		cmp[2] = strcmp(a[indexA].reference, b[indexB].reference);
		cmp[3] = (a[indexA].chromosome <= b[indexB].chromosome)?((a[indexA].chromosome<b[indexB].chromosome)?-1:0):1;
		cmp[4] = (a[indexA].position <= b[indexB].position)?((a[indexA].position<b[indexB].position)?-1:0):1;
		cmp[5] = (a[indexA].strand <= b[indexB].strand)?((a[indexA].strand<b[indexB].strand)?-1:0):1;

		/* ingenious */
		result = 0;
		for(i=0;i<6;i++) {
			result += pow(10, 8-i-1)*cmp[i];
		}

		if(result < 0) {
			return -1;
		}
		else if(result == 0) {
			return 0;
		}
		else {
			return 1;
		}
	}
	else {
		assert(sortOrder == AlignEntrySortByChrPos);
		cmp[0] = (a[indexA].chromosome <= b[indexB].chromosome)?((a[indexA].chromosome<b[indexB].chromosome)?-1:0):1;
		cmp[1] = (a[indexA].position <= b[indexB].position)?((a[indexA].position<b[indexB].position)?-1:0):1;
		cmp[2] = (a[indexA].referenceLength <= b[indexB].referenceLength)?((a[indexA].referenceLength<b[indexB].referenceLength)?-1:0):1;
		result = 0;
		for(i=0;i<3;i++) {
			result += pow(10, 8-i-1)*cmp[i];
		}

		if(result < 0) {
			return -1;
		}
		else if(result == 0) {
			return 0;
		}
		else {
			return 1;
		}
	}
}

/* TODO */
int AlignEntryGetOneRead(AlignEntry **entries, FILE *fp)
{
	char *FnName="AlignEntryGetOne";
	int cont = 1;
	int numEntries = 0;
	fpos_t position;
	AlignEntry temp;

	/* Initialize temp */
	temp.read = malloc(sizeof(char)*SEQUENCE_LENGTH);
	if(NULL==temp.read) {
		PrintError(FnName,
				"temp.read",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	temp.reference = malloc(sizeof(char)*SEQUENCE_LENGTH);
	if(NULL==temp.reference) {
		PrintError(FnName,
				"temp.reference",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Try to read in the first entry */
	if(EOF != AlignEntryRead(&temp, fp)) {

		/* Initialize the first entry */
		numEntries++;
		(*entries) = malloc(sizeof(AlignEntry)*numEntries);
		if(NULL == (*entries)) {
			PrintError(FnName,
					"(*entries)",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		(*entries)[numEntries-1].read = malloc(sizeof(char)*SEQUENCE_LENGTH);
		if(NULL==(*entries)[numEntries-1].read) {
			PrintError(FnName,
					"(*entries)[numEntries-1].read",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		(*entries)[numEntries-1].reference = malloc(sizeof(char)*SEQUENCE_LENGTH);
		if(NULL==(*entries)[numEntries-1].reference) {
			PrintError(FnName,
					"(*entries)[numEntries-1].reference",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		/* Copy over from temp */
		AlignEntryCopy(&temp, &(*entries)[numEntries-1]);

		/* Save current position */
		fgetpos(fp, &position);

		while(cont == 1 && EOF != AlignEntryRead(&temp, fp)) {
			/* Check that the read name matches the previous */
			if(strcmp(temp.readName, (*entries)[numEntries-1].readName)==0) {
				/* Save current position */
				fgetpos(fp, &position);
				/* Copy over */
				numEntries++;
				(*entries) = realloc((*entries), sizeof(AlignEntry)*numEntries);
				if(NULL == (*entries)) {
					PrintError(FnName,
							"(*entries)",
							"Could not reallocate memory",
							Exit,
							ReallocMemory);
				}
				(*entries)[numEntries-1].read = malloc(sizeof(char)*SEQUENCE_LENGTH);
				if(NULL==(*entries)[numEntries-1].read) {
					PrintError(FnName,
							"(*entries)[numEntries-1].read",
							"Could not allocate memory",
							Exit,
							MallocMemory);
				}
				(*entries)[numEntries-1].reference = malloc(sizeof(char)*SEQUENCE_LENGTH);
				if(NULL==(*entries)[numEntries-1].reference) {
					PrintError(FnName,
							"(*entries)[numEntries-1].reference",
							"Could not allocate memory",
							Exit,
							MallocMemory);
				}
				/* Copy over from temp */
				AlignEntryCopy(&temp, &(*entries)[numEntries-1]);
			}
			else {
				/* Exit the loop */
				cont = 0;
				/* Reset position in the file */
				fsetpos(fp, &position);
			}
		}
	}

	/* Free memory */
	assert(temp.read!=NULL);
	free(temp.read);
	assert(temp.reference!=NULL);
	free(temp.reference);

	return numEntries;
}

/* TODO */
int AlignEntryGetAll(AlignEntry **entries, FILE *fp)
{
	char *FnName="AlignEntryGetAll";
	int numEntries = 0;
	AlignEntry temp;

	/* Initialize temp */
	temp.read = malloc(sizeof(char)*SEQUENCE_LENGTH);
	if(NULL==temp.read) {
		PrintError(FnName,
				"temp.read",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	temp.reference = malloc(sizeof(char)*SEQUENCE_LENGTH);
	if(NULL==temp.reference) {
		PrintError(FnName,
				"temp.reference",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Try to read in the first entry */
	numEntries=0;
	while(EOF != AlignEntryRead(&temp, fp)) {

		/* Initialize the first entry */
		numEntries++;
		(*entries) = realloc((*entries), sizeof(AlignEntry)*numEntries);
		if(NULL == (*entries)) {
			PrintError(FnName,
					"(*entries)",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}
		(*entries)[numEntries-1].read = malloc(sizeof(char)*SEQUENCE_LENGTH);
		if(NULL==(*entries)[numEntries-1].read) {
			PrintError(FnName,
					"(*entries)[numEntries-1].read",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		(*entries)[numEntries-1].reference = malloc(sizeof(char)*SEQUENCE_LENGTH);
		if(NULL==(*entries)[numEntries-1].reference) {
			PrintError(FnName,
					"(*entries)[numEntries-1].reference",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		/* Copy over from temp */
		AlignEntryCopy(&temp, &(*entries)[numEntries-1]);
	}

	/* Free memory */
	assert(NULL!=temp.read);
	free(temp.read);
	assert(NULL!=temp.reference);
	free(temp.reference);

	return numEntries;
}

void AlignEntryCopy(AlignEntry *src, AlignEntry *dst)
{
	strcpy(dst->readName, src->readName);
	strcpy(dst->read, src->read);
	strcpy(dst->reference, src->reference);
	dst->length = src->length;
	dst->referenceLength = src->referenceLength;
	dst->chromosome = src->chromosome;
	dst->position = src->position;
	dst->strand = src->strand;
	dst->score = src->score;
}
