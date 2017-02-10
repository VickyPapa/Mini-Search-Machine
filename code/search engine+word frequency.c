/*
How to use:
Please change the DATAPATH in define module according to where you put data in.

In general, what this program do are:
	Get words from files.
 -> Filter and sort words into terms.
 -> Record occurences of terms in table.
 -> Search word of query. 
*/

/*
Header file declaration.
*/
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "io.h"
#include "direct.h" //You can use "direct.h" or "dir.h" depengding on environment. 
#include "stem.h"
#include <windows.h>

/*
Define constant.
*/
#define DATAPATH ".\\data"
#define STOPWORDFREQUENCY 0.0023//Parameter to find out a stop word.
#define STOPWORDSTART 1000000 //Parameter to find out a stop word.
#define StopWordNumber 50000 //Max number of stop words.
#define MaxFileNumber 900 //Max number of files.
#define FILENAMELENGTH 30 //Max length of file name.
#define BufferSize 500 //Size of buffer.
#define WordLength 30 //Suppose words are not longer than 30.
#define max_length 50	// Suppose input word or phrase are no longer than 50
#define max_num 10	// Suppose the number of words in a phrase are no longer than 10
#define Legitimate 1 //State of TableNode: Legitimate.
#define Empty 0  //State of TableNode: Empty.

/*
Global variable declaration.
*/
int TotalWordCount=0; //Number of total words.
int StopWordNo=0;
int PrimeNumberTable[] = { 10159,20533,41233,82471,165719,348937,700001,1400017 }; //Table of prime numbers.
char StopWordTable[StopWordNumber][WordLength];

/*
Structure declaration.
*/
typedef struct FileNode* File;
struct FileNode {
	FILE* Location; //Pointer to where the file is.
	int FileNumber; //Position of file in a database.
};

typedef struct  OffsetNode* Offset;
struct OffsetNode {
	int OffsetInFile; //Location in the file.
	Offset NextOffset; //Pointer to next Location of the term.
};

typedef struct RecordNode* Record;
struct RecordNode {
	int LocalFrequency; //Occurrence number of this term in the file.
	int HostFile; //Numeber of the file it belongs to.
	Offset FileOffset; //Location in the file.
	Record NextRecord; //Pointer to next record of the term.
};

typedef struct TermNode* Term;
struct TermNode {
	char Content[WordLength]; //Term as string.
	int TotalFrequency; //Occurrence number of this term in the database.
	int TotalFiles;			//Occurrence number of files which contain this term
	Record RecordList; //Pointer to a list of its appearance records.
};

typedef struct TableNode* Table;
struct TableNode {
	int State; //Mark whether it is empty or not.
	Term Word; //Term in this node.
};

/*
Functions declaration.
*/
extern int stem(char * p, int i, int j); //Get the stem of a word.
char* WordToTerm(char* word); //Pre-process word into term
int IsStopWord(char* word); //Determine whether it is a stop word
int Hashing(char* word, int* TotalTableSize); //Hash function.
void Insert(Table** Table_1, char* word, int fileNo, int lineNo, int* CurrentTableSize, int* TotalTableSize, int* WhichPrime);  //Insert an input.
int Find(char* word, Table** Table_1, int* TotalTableSize, int* WhichPrime); //Find the position of a word.
void Rehashing(Table** Table_1, int* TotalTableSize, int* WhichPrime); //Rehashing when half of the table is full.
void my_strcpy(char* s1, char* s2, int m, int n);//string copy operation
void sort(Record *List, int num);	//sort the document ID by the frequency of the term
Term Query_word(char *word, Table** Table_1, int* TotalTableSize, int* WhichPrime);	//find the proper position of a term in the data base
void Query(char* word, float threshold, Table** Table_1, int* TotalTableSize, int* WhichPrime);	//solve 2 different situations of query for both word and phrases
void WriteOut(Table** Table_1, int* TotalTableSize,char* filename); //Write out current table into hard disk.
void DeleteTable(Table** Table_1, int* TotalTableSize); //Delete table to free space.
void DeleteStopWord(Table **Table_1,int pos); //Delete stop word from table.

/*
Pre-process word into term.
Functions as follow:
1、If it is upper case, transform it to lower case.
2、Get the stem of a word.
*/
char* WordToTerm(char* word) { //Pre-process word into term
	int i;

	for (i = 0; (word[i] != '\0') && (i < 30); i++) {
		if ((word[i] >= 'A') && (word[i] <= 'Z')) { 
			word[i] = word[i] - 'A' + 'a'; //upper case->lower case.
		}
	}	

    i--;
	i = stem(word, 0, i); 
	word[i + 1] = '\0'; //Get the stem of a word.
	return word;
}

/*
Find out whether it is a stop word.
Way to do that:
Search the word in stopWordTable by binary search since the stopWordTable is sorted.
*/
int IsStopWord(char* word) {
	int i;
	for(i=0;i<StopWordNo;i++){
		if(strcmp(StopWordTable[i], word)==0){
			return 1;
		}
	}
	return 0;
}

/*
Get the key of hash.
Way to do that:
Using quadratic probing.
*/
int Hashing(char* word, int* TotalTableSize) {
	int result = 0, i; 

	//the algorithm to calculate the hash value
	for (i = 0; (word[i] != '\0') && (i < 30); i++) {
		result = 32 * result + word[i];//Using 32 as weight of a char,
                                        //and multipling 32 is indeed shifting bits, which is fast in hardware.
	}
	result = result % (*TotalTableSize);
	if (result < 0) { //Overflow may make the result negative, so here is a makeup.
		result = result + (*TotalTableSize);
	}
	return result;
}

/*
Record one time ocurrence of word.
Possible situations:
1、It is a new word.
2、It is already in table but first time in this file.
3、It is already in table and also in this file.
*/
void Insert(Table** Table_1, char* word, int fileNo, int WordOffset, int* CurrentTableSize, int* TotalTableSize, int* WhichPrime) {
	int pos,flag = 1;
	Record CurrentRecord;
	Term NewTerm = NULL;
	Record NewRecord = NULL;
	Offset NewOffset=NULL;
	 
	TotalWordCount++; //Increase number of total words.

	word=WordToTerm(word); //Make word standardization.
	pos = Find(word, Table_1, TotalTableSize, WhichPrime);  //Get the position.	
	
	if(IsStopWord(word)){ //It is already stop word which is already sorted out.
		return; //Do nothing.
	}
	
	if ((*Table_1)[pos]->State != Legitimate) { //It is a new word.
		(*CurrentTableSize)++;
		(*Table_1)[pos]->State = Legitimate;
		NewTerm = (Term)malloc(sizeof(struct TermNode));
		(*Table_1)[pos]->Word = NewTerm;
		strcpy(NewTerm->Content,word);
		NewTerm->TotalFrequency = 1;
		NewTerm->TotalFiles = 1;
		NewRecord = (Record)malloc(sizeof(struct RecordNode));
		NewTerm->RecordList = NewRecord;
		NewRecord->LocalFrequency = 1;
		NewRecord->NextRecord = NULL;
		NewRecord->HostFile = fileNo;
		NewOffset = (Offset)malloc(sizeof(struct OffsetNode));
		NewRecord->FileOffset = NewOffset;
		NewOffset->OffsetInFile = WordOffset;
		NewOffset->NextOffset = NULL;
	}
	else if ((*Table_1)[pos]->State == Legitimate) { //It is already in hashtable.
		if(TotalWordCount>STOPWORDSTART&&((*Table_1)[pos]->Word->TotalFrequency/(double)TotalWordCount)>STOPWORDFREQUENCY) { //It is already stop word which is newly sorted out.
			strcpy(StopWordTable[StopWordNo++],word); //Add it to stop table.
			DeleteStopWord(Table_1,pos); //Delete it from table.
			return;
		}
		((*Table_1)[pos]->Word->TotalFrequency)++;
		CurrentRecord = (*Table_1)[pos]->Word->RecordList;
		while (CurrentRecord != NULL) { 
			if (CurrentRecord->HostFile == fileNo) {
				flag = 0;
				break;
			}
			CurrentRecord = CurrentRecord->NextRecord;
		}
		if (flag) { //It is the first time it appears in this file.
			((*Table_1)[pos]->Word->TotalFiles)++;
			NewRecord = (Record)malloc(sizeof(struct RecordNode));
			NewRecord->NextRecord = (*Table_1)[pos]->Word->RecordList;
			(*Table_1)[pos]->Word->RecordList = NewRecord;
			NewRecord->LocalFrequency = 1;
			NewRecord->HostFile = fileNo;
			NewRecord->FileOffset = NULL;
			NewOffset = (Offset)malloc(sizeof(struct OffsetNode));
			NewOffset->NextOffset = NewRecord->FileOffset;
			NewRecord->FileOffset = NewOffset;
			NewOffset->OffsetInFile = WordOffset;
		}
		else {// It has appeared in this file.
			(CurrentRecord->LocalFrequency)++;
			NewOffset = (Offset)malloc(sizeof(struct OffsetNode));
			NewOffset->NextOffset = CurrentRecord->FileOffset;
			CurrentRecord->FileOffset = NewOffset;
			NewOffset->OffsetInFile = WordOffset;
		}
	}	
	return;
}

/*
Find whether and where is the word in hash table.
Way to deal with collision:
Open Addressing & Quadratic Probing.
*/
int Find(char* word, Table** Table_1, int* TotalTableSize, int* WhichPrime) {
	int CurrentPos;   //Key, 
	int CollisionNum = 0;
	int test=0;
     
	CurrentPos = Hashing(word, TotalTableSize); //Initial hash key.
	while (((*Table_1)[CurrentPos]->State==Legitimate) && ((strcmp((*Table_1)[CurrentPos]->Word->Content,word)!=0))) {
		CurrentPos += 2 * ++CollisionNum - 1;  //Equal to square operation. 
		CurrentPos = CurrentPos % (*TotalTableSize);
		if (CollisionNum > ((*TotalTableSize) / 2 + 1)) { //Load density > 0.5
			Rehashing(Table_1, TotalTableSize, WhichPrime);
			CollisionNum = 0;  //Reset CollisionNum. 
			CurrentPos = Hashing(word, TotalTableSize);  //Reset CurrentPos.
		}
	}
	return CurrentPos;
}

/*
Rehashing.
Trigger: Load density of hash table > 0.5
*/
void Rehashing(Table** Table_1, int* TotalTableSize, int* WhichPrime) {
	int newpos, i,j, temp = *TotalTableSize; //Temp keeps the last size.	
	Table* temptable;
    
    temptable=*Table_1;
    *WhichPrime = *WhichPrime + 1; //Next prime.
	*TotalTableSize = PrimeNumberTable[*WhichPrime]; //New tablesize.
	(*Table_1) = (Table*)malloc((*TotalTableSize)*sizeof(Table)); //Establish new table.
	for(j=0;j<(*TotalTableSize);j++){
		(*Table_1)[j] = (Table)malloc(sizeof(struct TableNode)); 
	}
	for(j=0;j<(*TotalTableSize);j++){  //Initialize.
		(*Table_1)[j]->State=Empty;
	}
	for (i = 0; i < temp; i++) {
		if (temptable[i]->State == Legitimate) {  //If it has a term, put it to newtable
			newpos = Find(temptable[i]->Word->Content, Table_1,TotalTableSize,WhichPrime);
			(*Table_1)[newpos] = temptable[i];	
		}
	}
	free(temptable);
	
	return;
}

/*copy s2[m]~s2[n-1] to s1*/
void my_strcpy(char* s1, char* s2, int m, int n){
	int i;
	for (i = 0; i + m < n; i++)
		s1[i] = s2[m + i];
	s1[i] = '\0';
}

Term Query_word(char *word, Table** Table_1, int* TotalTableSize, int* WhichPrime){
	int pos;
	/*tunrns to lower case, solve stemming and noisy word */
	word = WordToTerm(word);
	if (IsStopWord(word)) { //Filter stop words.
		strcpy(word," ");
	}
	if (strcmp(word," ")==0){	//it's a noisy word
		printf("Noisy Word!\n");
		return NULL;
	}
		/*find the position in the hash table*/
	else{
		pos = Find(word, Table_1, TotalTableSize, WhichPrime);	

		/*if it's not in the hash table*/
		if ((*Table_1)[pos]->State != Legitimate){
			printf("Not Found!\n");
			return NULL;
		}
		else
			return (*Table_1)[pos]->Word;	//return the position in the hash table
	}
}

/*Bubble sort to sort the record list by local frequency decreasingly*/
void sort(Record *List, int num){
	int i, j;
	for (i = 0; i < num; i++){
		for (j = i; j < num; j++){
			if (List[i]->LocalFrequency < List[j]->LocalFrequency){
				Record newRecord = List[j];
				List[j] = List[i];
				List[i] = newRecord;
			}
		}
	}
}

void Query(char* word, float threshold, Table** Table_1, int* TotalTableSize, int* WhichPrime){
	char word_temp[max_num][max_length];
	int flag = 0;	// flag marks whether the input is a word or a phrase
	int i = 0, j = 0, k=0;
	int initial=-1;
	int num = 0;	// num represent the number of words in the phrase
	Term Foundterm;	
	Term *Foundterms;
	Record PresentRecord;
	Record* List;
	Record* CurrentRecord;	// use to find whether the documents contain the phrase
	Offset* CurrentOffset;	//// use to find whether the documents contain the phrase
	int count;
	int NotRightFile=0;
	int NotFound=1;
	int wordnum=0;

	/* if the input is a phrase, copy all the words into word_temp[][] */

	j = -1;	// j represent the last index of space
	while (word[i] != '\0'){
		if (word[i] == ' '||word[i]== ' ' ||word[i]== '\'' ||word[i]== '-' ||word[i]== '[' ||word[i]== ']' ||word[i]== '|' ||word[i]== '\"'||word[i]== '.' ||word[i]== '\?' ||word[i]== '!' ||word[i]== ':' ||word[i]== ';'||word[i]== ',' ){		// i represent the present index of space
			if (i - j>1){
				flag = 1;
				/*copy a word into s_temp[num][] */
				my_strcpy(word_temp[num++], word, j + 1, i);
				word[i] = '\0';
			}
			j = i;
		}
		i++;
	}
	if (i - j>1)
		my_strcpy(word_temp[num++], word, j + 1, i);

	/* if the input is a phrase */
	if (flag == 1){
		/*set aside some space for Foundterms[], CurrentRecord[] and CurrentOffset[] */
		/*num represent the number of words in a phrase*/
		Foundterms = (Term *)malloc(sizeof(Term)* num);
		CurrentRecord = (Record *)malloc(sizeof(Record)*num);	
		CurrentOffset = (Offset *)malloc(sizeof(Offset)*num);
		for (i = 0; i<num; i++){
			Foundterms[i] = Query_word(word_temp[i], Table_1, TotalTableSize, WhichPrime);
			/*deal with noisy word or not found word*/
			if (Foundterms[i] == NULL){
				CurrentRecord[i] = NULL;
				CurrentOffset[i] = NULL;
				continue;			
			}
			/*CurrentRecord[i] represent the current record of the ith word in the phrase*/
			/*CurrentOffset[i] represent the current offset of the ith word in the phrase*/
			else {
				/*avoid starting from noisy word or not found word*/
				if(initial == -1)
					initial =i;
				CurrentRecord[i] = Foundterms[i]->RecordList;
				CurrentOffset[i] = CurrentRecord[i]->FileOffset;
				wordnum++;
			}
		}
		if(wordnum == 0)
			return;
		count = 0;	// count the number of documents
		for (j = 0; j < Foundterms[initial]->TotalFiles; j++){	// go through the recordlist of the first word in the phrase
			for (k = 0; k < CurrentRecord[initial]->LocalFrequency; k++){	// go through the position of the first word in a specific file
				for (i = 0; i < num; i++){	// go through the rest words in the phrase
					/*the records are stored by documentID decreasingly*/
					while (CurrentRecord[i] != NULL && CurrentRecord[i]->HostFile > CurrentRecord[initial]->HostFile){
							CurrentRecord[i] = CurrentRecord[i]->NextRecord;
							if(CurrentRecord[i] !=NULL)
								CurrentOffset[i] = CurrentRecord[i]->FileOffset;
					}
					/*could not find a word in the database anymore, all the documents contain the phrase has been printed out */
					if (CurrentRecord[i] == NULL){
						//if(NotFound ==1)	// no documents ID has been printed out before
						//	printf("Not Found!");
						//	return ;
						continue;
					}
					/*find the file contains both word[0] and word[i]*/
					else if (CurrentRecord[i]->HostFile == CurrentRecord[initial]->HostFile){

						/*the offset are stored decreasingly*/
						while (CurrentOffset[i] != NULL && CurrentOffset[i]->OffsetInFile > (CurrentOffset[initial]->OffsetInFile + i - initial ))
								CurrentOffset[i] = CurrentOffset[i]->NextOffset;
						
						/*could not find the phrase in this file*/
						if (CurrentOffset[i] == NULL){
							NotRightFile = 1;		//mark in order to jump out 2 level of loops
							break;
						}
						/*find the offset that offset(word[i]) = offset(word[0])+i*/
						else if (CurrentOffset[i]->OffsetInFile != CurrentOffset[initial]->OffsetInFile + i-initial )
							break;
					
					}
					else{
						/*could not find the phrase in this file*/
						NotRightFile = 1;
						break;
					}
				}
				/*the file contains the phrase*/
				if (i == num && count < threshold){
					NotFound =0;
					printf("%7d", CurrentRecord[initial]->HostFile);
					count++;
					/*output 10 files in a line*/
					if (count % 10 == 0)
						printf("\n");
					NotRightFile = 1;
				}
				/*jump out the second level of loops*/
				if(NotRightFile ==1 ){
					NotRightFile =0;
					break;
				}
				/*search the next offset of word[0]*/
				else CurrentOffset[initial] = CurrentOffset[initial]->NextOffset;
			}
			/*search the next file of word[0]*/
			CurrentRecord[initial] = CurrentRecord[initial]->NextRecord;
			if(CurrentRecord[initial] !=NULL)
				CurrentOffset[initial] = CurrentRecord[initial]->FileOffset;
		}
		if(NotFound ==1 )
			printf("Not Found!\n");
	}

	/* the input is a word */
	else{
		Foundterm = Query_word(word, Table_1, TotalTableSize, WhichPrime);
		if (Foundterm == NULL)
			return;
		if( threshold < 1 )
			threshold = threshold * Foundterm->TotalFiles;
		/*store the record list into an array List[]*/
		List = (Record*)malloc(sizeof(Record)* Foundterm->TotalFiles);
		PresentRecord = Foundterm->RecordList;
		for (i = 0; i<Foundterm->TotalFiles; i++){	
			List[i] = PresentRecord;
			PresentRecord = PresentRecord->NextRecord;
		}
		/*sort the record list by the local frequency decreasingly*/
		sort(List,i);
		/*output 8 files in a line*/
		PresentRecord = Foundterm->RecordList;
		for (count = 0; count <  Foundterm->TotalFiles && count < threshold; count++){
			if (count % 8 == 0)
				printf("\n");
					//	print document ID and frequency in the document
				printf("%4d(%3d)", List[count]->HostFile, List[count]->LocalFrequency);
		}
	}
}


/*
Delete stop word.
Trigger: Frequency of a word > 1000/TotalWordCount.
*/
void DeleteStopWord(Table **Table_1,int pos){
	Record currentrecord,freerecord;
	Offset currentoffset,freeoffset;
	
	(*Table_1)[pos]->State=Empty;
	
	currentrecord=(*Table_1)[pos]->Word->RecordList; 
	while(currentrecord!=NULL){
		freerecord=currentrecord;
		currentrecord=currentrecord->NextRecord;
		currentoffset=freerecord->FileOffset; 
		while(currentoffset!=NULL){
			freeoffset=currentoffset;
			currentoffset=currentoffset->NextOffset;
			free(freeoffset);
		}		
		free(freerecord);
	}		
	return;
}

/*
Write currrent table into hard disk.
Trigger: Numbers of file > 1000.
*/
void WriteOut(Table** Table_1, int* TotalTableSize, char* filename){
	int i;
	Record currentrecord;
	Offset currentoffset;
	FILE *fp;
				
	fp=fopen(filename, "w+");		
	/* Output current table. */
	for(i=0;i<(*TotalTableSize);i++){
		if((*Table_1)[i]->State==Legitimate){
			fprintf(fp,"Term:%s\n",(*Table_1)[i]->Word->Content);
			fprintf(fp,"TotalFrequency:%d\n",(*Table_1)[i]->Word->TotalFrequency);
			currentrecord=(*Table_1)[i]->Word->RecordList;
			while(currentrecord!=NULL){
				fprintf(fp,"HostFile:%d\n",currentrecord->HostFile);
				fprintf(fp,"LocalFrequency:%d\n",currentrecord->LocalFrequency);
				currentoffset=currentrecord->FileOffset; 
				while(currentoffset!=NULL){
					fprintf(fp,"FileOffset:%d\n",currentoffset->NextOffset);
					currentoffset=currentoffset->NextOffset;
				}					
				currentrecord=currentrecord->NextRecord;
			}				
		}
	}
	/* Output ending. */		
    fclose(fp);   
    return;
}

/*
Delete currrent table.
Trigger: Numbers of file > 1000.
*/
void DeleteTable(Table** Table_1, int* TotalTableSize){
	int i;
	Record currentrecord,freerecord;
	Offset currentoffset,freeoffset;	
	
	for(i=0;i<(*TotalTableSize);i++){
		if((*Table_1)[i]->State==Legitimate){
			currentrecord=(*Table_1)[i]->Word->RecordList; 
			while(currentrecord!=NULL){
				freerecord=currentrecord;
				currentrecord=currentrecord->NextRecord;
				currentoffset=currentrecord->FileOffset; 
				while(currentoffset!=NULL){
					freeoffset=currentoffset;
					currentoffset=currentoffset->NextOffset;
					free(freeoffset);
				}
				free(freerecord);
			}	
		}
		free((*Table_1)[i]);
	}
	free(*Table_1);
	
	return;
}

int main() {
	int a=0,b,c=0;
	int *WhichPrime = &a, *TotalTableSize=&b, *CurrentTableSize=&c;
	Table** Table_1;
	Table* TableArray;
	int FileNo = 0, WordOffset = 0; //Postion of file and word.	
	char InputWord[WordLength];
	long file;
	struct _finddata_t find;
	struct FileNode data[MaxFileNumber];
	char temp;
	int i,j,k, flag = 1;
	char s[max_length]; // store the input word or phrase.
	float threshold;
	FILE* fp; //Pointer to output file.
	char filename[FILENAMELENGTH]; //store output filename.
	int index=0,digit=1;	
	
	/*Initialize stopword table.*/
	for(k=0;k<StopWordNumber;k++){
		StopWordTable[k][0]='\n';
	}
	
	/*Initialize output filename.*/
	strcpy(filename,"table0");
	strcat(filename,".txt");

	/*begin to build the inverted file index*/
	*TotalTableSize = PrimeNumberTable[*WhichPrime];
	TableArray = (Table*)malloc((*TotalTableSize)*sizeof(Table));
	Table_1=&TableArray;
	for(j=0;j<(*TotalTableSize);j++){
		(*Table_1)[j] = (Table)malloc(sizeof(struct TableNode)); //Establish table.
	}
	for(j=0;j<(*TotalTableSize);j++){
		(*Table_1)[j]->State=Empty;
	}
	
	_chdir(DATAPATH);  //Change it if you put data in other directory.
	if ((file = _findfirst("*.txt", &find)) == -1L)
	{
		printf("Directory is empty!\n");
		exit(0);
	}
	data[FileNo].FileNumber = FileNo; //Number each file.
	data[FileNo].Location = fopen(find.name, "r"); //Open file.
	fseek(data[FileNo].Location, 0L, SEEK_SET); //Make file pointer points to the beginning of file.
	//Get words from file.
	WordOffset = 0;
	while (flag) {
		i = 0;
		while (temp = fgetc(data[FileNo].Location)) {
			if (temp == EOF) {  //Reach the end of file.
				flag = 0;
				break;
			}
			//Ignore certain symbols.
			if (temp == ' ' ||temp == '\'' ||temp == '-' ||temp == '[' ||temp == ']' ||temp == '|' ||temp == '\"'|| temp == '.' || temp == '\?' || temp == '!' || temp == ':' || temp == ';'|| temp == ',' || temp == '\n') {
				break;
			}
			else {
				InputWord[i++] = temp;
			}
		}
		if (flag&&(i!=0)) {
			InputWord[i] = '\0'; //Mark the ending of a word.
			Insert(Table_1, InputWord, FileNo, WordOffset, CurrentTableSize, TotalTableSize, WhichPrime);
			WordOffset++;
		}
	}
	fclose(data[FileNo].Location);//Close file.
	FileNo++;
	while (_findnext(file, &find) == 0) {
		flag=1;
		data[FileNo].FileNumber = FileNo;
		data[FileNo].Location = fopen(find.name, "r"); //Open file.
		fseek(data[FileNo].Location, 0L, SEEK_SET); //Make file pointer points to the beginning of file.
		//Get words from file.
		WordOffset = 0;
		while (flag) {
			i = 0;
			while (temp = fgetc(data[FileNo].Location)) {
				if (temp == EOF) {  //Reach the end of file.
					flag = 0;
					break;
				}
				//Ignore certain symbols.
				if (temp == ' ' ||temp == '\'' ||temp == '-' ||temp == '[' ||temp == ']' ||temp == '|' ||temp == '\"'|| temp == '.'|| temp == ',' || temp == '\?' || temp == '!' || temp == ':' || temp == ';'|| temp == ',' || temp == '\n') {
					
					break;
				}
				else {
					InputWord[i++] = temp;
				}
			}
			if (flag) {
				InputWord[i] = '\0'; //Mark the ending of a word.
				Insert(Table_1, InputWord, FileNo, WordOffset, CurrentTableSize, TotalTableSize, WhichPrime);
				WordOffset++;
			}
		}
		fclose(data[FileNo].Location);//Close the file
		FileNo++;
		
		/*In order to handle huge data, write out table into hard disk every 1000 data files.*/
		if(FileNo%1000==0){ 
			WriteOut(Table_1,TotalTableSize,filename);
			/*Prepare for the next output filename.*/
			index++;
    		if(index%((int)pow(10,digit))==0){ 
    			strncpy(filename,filename,5+digit);
    			filename[6+digit]='\0';
    			digit++;  		
    			filename[3+digit]='1';
    			filename[4+digit]='0'-1;
    			strcat(filename,".txt");
			}
			filename[4+digit]++;
			/*Prepare ending.*/
				
			DeleteTable(Table_1,TotalTableSize);
			
			/*Initialize new table.*/
			*WhichPrime=0;
			*TotalTableSize = PrimeNumberTable[*WhichPrime];
			TableArray = (Table*)malloc((*TotalTableSize)*sizeof(Table));
			Table_1=&TableArray;
			for(j=0;j<(*TotalTableSize);j++){
				(*Table_1)[j] = (Table)malloc(sizeof(struct TableNode)); //Establish table.
			}
			for(j=0;j<(*TotalTableSize);j++){
				(*Table_1)[j]->State=Empty;
			}
		}
						
	}
	_findclose(file);

	/*begin to do the query program*/
	/*input the word and threshold*/
	while(1){
        if(flag!=0){
            printf("\ninput 1 for continue, 0 for exit\n");
            scanf("%d",&j);
            if(!j)
                break;
        }   
	    printf("Please input a word or a phrase: ");
	    for(i=0;i<max_length;i++)
	        s[i]=0;
        if(flag!=0)
            getchar();
	    gets(s);
	    printf("Please input an integer or a floating point (percentage) as the threshold: ");
	    scanf("%f", &threshold);
	    Query(s,threshold, Table_1, TotalTableSize, WhichPrime);
        flag=1;
     }
     system("PAUSE"); 
     return 0;     

}

