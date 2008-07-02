#ifndef ALIGNENTRY_H_
#define ALIGNENTRY_H_

#include <stdio.h>
#include "BLibDefinitions.h"
#include "RGBinary.h"

enum {AlignEntrySortByAll, AlignEntrySortByChrPos};

typedef struct {
	char *read; /* The read */
	char *reference;
	unsigned int length; /* The length of the alignment */
	unsigned int referenceLength; /* The number of bases excluding gaps in the reference string */
	int32_t chromosome;
	int32_t position;
	char strand;
	double score;
} AlignEntry;

int AlignEntryPrint(AlignEntry*, FILE*);
int AlignEntryRead(AlignEntry*, FILE*);
int AlignEntryRemoveDuplicates(AlignEntry**, int, int);
void AlignEntryQuickSort(AlignEntry**, int, int, int, int, double*, int);
void AlignEntryMergeSort(AlignEntry**, int, int, int, int, double*, int);
void AlignEntryCopyAtIndex(AlignEntry*, int, AlignEntry*, int);
int AlignEntryCompareAtIndex(AlignEntry*, int, AlignEntry*, int, int);
int AlignEntryGetOneRead(AlignEntry**, FILE*);
int AlignEntryGetAll(AlignEntry**, FILE*);
void AlignEntryCopy(AlignEntry*, AlignEntry*);
void AlignEntryFree(AlignEntry*);
void AlignEntryInitialize(AlignEntry*);
void AlignEntryCheckReference(AlignEntry*, RGBinary*);
int AlignEntryGetPivot(AlignEntry*, int, int, int);
#endif
