#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_WORDS 256
#define MAX_FILE_SIZE 2147483647
#define DEBUG

enum programState
{
    COMPRESS,
    DECOMPRESS,
    ERR_CANT_OPEN_FILE,
    ERR_TOO_MANY_ARGUMENTS,
    ERR_NO_ARGUMENT,
    ERR_WRONG_ARGUMENT,
    ERR_FILE_TOO_BIG,
    COMPRESSION_FINISHED,
    DECOMPRESSION_FINISHED
};

struct binTree
{
    struct binTree* parent;
    struct binTree* left;
    struct binTree* right;
    unsigned int freq;
    unsigned char value;
};

struct encodedSymbol
{
    char symbol[256];
    int length;
};

void printFreqTable(unsigned int* freqTable)
{
    int i;
    printf("\nFrequency Table (value, frequency):\n");
    for(i = 0; i < N_WORDS; i += 4)
    {
        printf("\t%x\t%i", i, freqTable[i]);
        printf("\t\t%x\t%i", i+1, freqTable[i+1]);
        printf("\t\t%x\t%i", i+2, freqTable[i+2]);
        printf("\t\t%x\t%i\n", i+3, freqTable[i+3]);
    }
}

void printProgress(char* title, int percent)
{
    int i, filled = 0;

    printf("\r");
    printf("%s [", title);
    for(i = 0; i < percent/5; i++, filled++)
        printf("#");
    for(i = 0; i < 20-filled; i++)
        printf("-");
    printf("] %i%%", percent);
}

void printEncodedTable(struct encodedSymbol* encodedTable)
{
    int i;
    printf("\nEncoded Table (value, code):\n");
    for(i = 0; i < N_WORDS; i++)
        printf("\t%x\t%s\n", i, encodedTable[i].symbol);
}

void createFreqTable(unsigned int* freqTable, FILE* inputFile, unsigned long int inputFileSize)
{
    unsigned char readBuffer;
    int i;
    clock_t elapsedTime = 0;

    for(i = 0; i < N_WORDS; i++)
        freqTable[i] = 0;

    while(fread(&readBuffer, sizeof(unsigned char), 1, inputFile))
    {
        freqTable[(unsigned int)readBuffer] += 1;
        if(clock() - elapsedTime > 500)
        {
            printProgress("Reading file:", (int)((double)ftell(inputFile) / inputFileSize * 100));
            elapsedTime = clock();
        }
    }
    printProgress("Reading file:", 100);
    printf("\n");
}

struct binTree* createTree(unsigned int freqTable[])
{
    int i;
    int numberOfTrees = N_WORDS;
    unsigned int minFreq1 = 0, minFreq2 = 0;
    int minFreq1Pos = -1,  minFreq2Pos = -1;
    /*creating N_WORDS subtrees - each for one value, we will merge them later on*/
    struct binTree* root[N_WORDS];
    struct binTree* tmpRoot = NULL;

    /*initializing subtrees for each value from 0 to 255*/
    for(i = 0; i < N_WORDS; i++)
    {
        root[i] = malloc(sizeof(struct binTree));
        root[i]->parent = NULL;
        root[i]->left = NULL;
        root[i]->right = NULL;
        root[i]->freq = freqTable[i];
        root[i]->value = (unsigned char)i;
    }

    /*finding subtrees with minimal frequency then joining them*/
    while(numberOfTrees > 1)
    {
        minFreq1Pos = -1;
        minFreq2Pos = -1;

        /*looking for 2 least frequent elements*/
        for(i = 0; i < N_WORDS; i++)
        {
            if(root[i] != NULL && minFreq1Pos == -1)
            {
                minFreq1 = root[i]->freq;
                minFreq1Pos = i;
            }
            else if(root[i] != NULL && root[i]->freq < minFreq1 )
            {
                minFreq1 = root[i]->freq;
                minFreq1Pos = i;
            }
        }
        for(i = 0; i < N_WORDS; i++)
        {
            if(root[i] != NULL && minFreq2Pos == -1 && i != minFreq1Pos)
            {
                minFreq2 = root[i]->freq;
                minFreq2Pos = i;
            }
            else if(root[i] != NULL && root[i]->freq < minFreq2 && i != minFreq1Pos)
            {
                minFreq2 = root[i]->freq;
                minFreq2Pos = i;
            }
        }
        /*creating new subtree from 2 minimal ones*/
        tmpRoot = malloc(sizeof(struct binTree));
        tmpRoot->parent = NULL;
        tmpRoot->left = root[minFreq1Pos];
        tmpRoot->right = root[minFreq2Pos];
        tmpRoot->freq = root[minFreq1Pos]->freq + root[minFreq2Pos]->freq;
        tmpRoot->value = 0;
        root[minFreq1Pos]->parent = tmpRoot;
        root[minFreq2Pos]->parent = tmpRoot;

        root[minFreq1Pos] = tmpRoot;
        root[minFreq2Pos] = NULL;

        numberOfTrees--;
    }
    return (root[minFreq1Pos]);
}

void searchTree(unsigned char symbol, struct encodedSymbol* encodedSymbol, struct binTree* root, char* path, int depth)
{
    int i;

    if(root == NULL)
        return;

    if(root->value == symbol && root->left == NULL && root->right == NULL)
    {
        for(i = 0; i < depth; i++)
            encodedSymbol->symbol[i] = path[i];

        encodedSymbol->length = depth;
        encodedSymbol->symbol[depth] = '\0';
        return;
    }

    if(root->left != NULL)
    {
        path[depth] = '0';
        searchTree(symbol, encodedSymbol, root->left, path, depth + 1);
    }

    if(root->right != NULL)
    {
        path[depth] = '1';
        searchTree(symbol, encodedSymbol, root->right, path, depth + 1);
    }
}

void encodeAndSaveFile(FILE* inputFile, FILE* outputFile, struct binTree* root, unsigned long int inputFileSize)
{
    int i;
    struct encodedSymbol encodedTable[N_WORDS];
    char path[N_WORDS];
    unsigned char readBuffer;
    unsigned char writeBuffer;
    unsigned char writeBuffFreeSpace;
    clock_t elapsedTime = 0;

    /*we have to create encoding table - encoded symbol for each 0-255 value */
    for(i = 0; i < N_WORDS; i++)
    {
        encodedTable[i].length = 0;
        searchTree((unsigned char)i, &(encodedTable[i]), root, path, 0);
    }
#ifdef DEBUG
    printEncodedTable(encodedTable);
#endif
    writeBuffFreeSpace = 8;
    writeBuffer = 0;
    fseek(inputFile, 0, SEEK_SET);

    while(fread(&readBuffer, sizeof(unsigned char), 1, inputFile))
    {
        for(i = 0; i < encodedTable[readBuffer].length; i++)
        {
            if(writeBuffFreeSpace == 0)
            {
                fwrite(&writeBuffer, sizeof(unsigned char), 1, outputFile);
                writeBuffFreeSpace = 8;
            }

            switch(encodedTable[readBuffer].symbol[i])
            {
                case '0':
                    writeBuffer = writeBuffer << 1;
                    writeBuffFreeSpace--;
                    break;

                case '1':
                    writeBuffer = writeBuffer << 1;
                    writeBuffer += 1;
                    writeBuffFreeSpace--;
                    break;
            }
        }
        if(clock() - elapsedTime > 500)
        {
            printProgress("Compressing file:", (int)((double)ftell(inputFile) / inputFileSize * 100));
            elapsedTime = clock();
        }
    }
    printProgress("Compressing file:", 100);
    printf("\n");

    /*we have to fill the remaining space with zeros and write last bit with info how many zeros were added*/
    if(writeBuffFreeSpace > 0)
    {
        writeBuffer = writeBuffer << writeBuffFreeSpace;
        fwrite(&writeBuffer, sizeof(unsigned char), 1, outputFile);
    }
    writeBuffer = writeBuffFreeSpace;
    fwrite(&writeBuffer, sizeof(unsigned char), 1, outputFile);
}

struct binTree* traverseTree(struct binTree* root, char bit)
{
    if(root == NULL)
        return NULL;

    if(bit == '0')
        return root->left;
    else if(bit == '1')
        return root->right;
    else
        return NULL;
}

char readBit(unsigned char byte, int position)
{
    /*position 0 means leftmost*/
    char result;
    /*bits just need to be flipped here - not sure why*/
    if((byte >> (7 - position)) & 1)
        result = '1';
    else
        result = '0';
    return (result);
}

void decodeAndSaveFile(FILE* inputFile, FILE* outputFile, struct binTree* root, unsigned long int inputFileSize)
{
    int endBit = 0;
    int readBufferReaminingBits;
    char bit;
    struct binTree* tmpTree;
    unsigned char endZerosToOmit;
    unsigned char readBuffer;
    unsigned char writeBuffer;
    unsigned long int tmpFilePos, inputFilePosition;
    clock_t elapsedTime = 0;

    tmpFilePos = ftell(inputFile);

    /*reading last byte to know how many zeros were added at the end*/
    fseek(inputFile, -1, SEEK_END);
    fread(&endZerosToOmit, sizeof(unsigned char), 1, inputFile);

    /*returning pointer to the previous position - after freq table*/
    fseek(inputFile, tmpFilePos, SEEK_SET);
    inputFilePosition = tmpFilePos;
    tmpTree = root;

    /*last byte is for storing omitted zeros, thus we dont read it now*/
    while(inputFilePosition < inputFileSize - 1)
    {
        fread(&readBuffer, sizeof(unsigned char), 1, inputFile);
        readBufferReaminingBits = 8;

        /*we need different behavior if we are at the last bit*/
        inputFilePosition = ftell(inputFile);
        if(inputFilePosition == inputFileSize - 1)
        {
            endBit = endZerosToOmit;
            /*printf("\nEOF - omitting %d zeros\n", endZerosToOmit );*/
        }

        /*we check tree until we find leaf or run out of bits from buffer*/
        while(readBufferReaminingBits > endBit)
        {
            bit = readBit(readBuffer, 8-readBufferReaminingBits);
            readBufferReaminingBits--;
            tmpTree = traverseTree(tmpTree, bit);

             /*we have found symbol only if our current node is a leaf*/
            if(tmpTree->left == NULL && tmpTree->right == NULL)
            {
                /*printf("\nSymbol found: %x \n", tmpTree->value );*/
                writeBuffer = tmpTree->value;
                fwrite(&writeBuffer, sizeof(unsigned char), 1, outputFile);
                tmpTree = root;
            }
        }
        if(clock() - elapsedTime > 500)
        {
            /*we need to divide everything by 1000 in order to avoid double overflow*/
            printProgress("Decompressing file:", (int)((double)inputFilePosition / inputFileSize * 100));
            elapsedTime = clock();
        }
    }
    printProgress("Decompressing file:", 100);
    printf("\n");
}

void destroyTree(struct binTree* root)
{
    if(root->left != NULL)
        destroyTree(root->left);
    if(root->right != NULL)
        destroyTree(root->right);
    free(root);
}

int main(int argc, char* argv[])
{
    enum programState programState;
    FILE* inputFile = NULL;
    FILE* outputFile = NULL;
    unsigned int freqTable[N_WORDS];
    unsigned long int inputFileSize;
    struct binTree* root = NULL;

    if(argc < 4)
        programState = ERR_NO_ARGUMENT;
    else if(argc > 4 )
        programState = ERR_TOO_MANY_ARGUMENTS;
    else if (*(argv[1]) == 'c')
        programState = COMPRESS;
    else if(*(argv[1]) == 'd')
        programState = DECOMPRESS;
    else
        programState = ERR_WRONG_ARGUMENT;

    switch(programState)
    {
        case COMPRESS:
            inputFile = fopen(argv[2], "rb");
            if(inputFile == NULL)
            {
                programState = ERR_CANT_OPEN_FILE;
                break;
            }
            /*checking file size*/
            fseek(inputFile, 0, SEEK_END);
            inputFileSize = ftell(inputFile);
            fseek(inputFile, 0, SEEK_SET);
            if(inputFileSize > MAX_FILE_SIZE)
            {
                programState = ERR_FILE_TOO_BIG;
                break;
            }
            outputFile = fopen(argv[3], "wb");
            if(inputFile == NULL)
            {
                programState = ERR_CANT_OPEN_FILE;
                break;
            }
            createFreqTable(freqTable, inputFile, inputFileSize);
#ifdef DEBUG
            printFreqTable(freqTable);
#endif
            root = createTree(freqTable);
            fwrite(&freqTable, sizeof(unsigned int), N_WORDS, outputFile);
            encodeAndSaveFile(inputFile, outputFile, root, inputFileSize);
            programState = COMPRESSION_FINISHED;
            break;

        case DECOMPRESS:
            inputFile = fopen(argv[2], "rb");
            if(inputFile == NULL)
            {
                programState = ERR_CANT_OPEN_FILE;
                break;
            }
            /*checking file size*/
            fseek(inputFile, 0, SEEK_END);
            inputFileSize = ftell(inputFile);
            fseek(inputFile, 0, SEEK_SET);
            if(inputFileSize > MAX_FILE_SIZE)
            {
                programState = ERR_FILE_TOO_BIG;
                break;
            }
            outputFile = fopen(argv[3], "wb");
            if(inputFile == NULL)
            {
                programState = ERR_CANT_OPEN_FILE;
                break;
            }
            fread(&freqTable, sizeof(unsigned int), N_WORDS, inputFile);
#ifdef DEBUG
            printFreqTable(freqTable);
#endif
            root = createTree(freqTable);
            decodeAndSaveFile(inputFile, outputFile, root, inputFileSize);
            programState = DECOMPRESSION_FINISHED;
            break;

        default:
            printf("Unknown error.\n");
            break;
    }

    /*error handling*/
    switch(programState)
    {
        case COMPRESSION_FINISHED:
            printf("File successfully compressed.\n");
            break;

        case DECOMPRESSION_FINISHED:
            printf("File successfully decompressed.\n");
            break;

        case ERR_NO_ARGUMENT:
            printf("Not enough arguments. Please use the following syntax: huffman.exe <task> \"source file\" \"destination file\"\nc - compress\nd - decompress\n");
            break;

        case ERR_TOO_MANY_ARGUMENTS:
            printf("Too many arguments. Please use the following syntax: huffman.exe <task> \"source file\" \"destination file\"\nc - compress\nd - decompress\n");
            break;

        case ERR_WRONG_ARGUMENT:
            printf("Wrong arguments. Please use the following syntax: huffman.exe <task> \"source file\" \"destination file\"\nc - compress\nd - decompress\n");
            break;
        case ERR_CANT_OPEN_FILE:
            printf("Can't open file.\n");
            break;
        case ERR_FILE_TOO_BIG:
            printf("File size must be below 2 GiB (max 2 147 483 647 bytes).\n32 bit limitation.\n");
            break;
        default:
            printf("Unknown error.\n");
            break;
    }

    /*cleanup*/
    destroyTree(root);
    fclose(inputFile);
    fclose(outputFile);

    return(0);
}
