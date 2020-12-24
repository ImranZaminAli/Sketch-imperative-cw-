#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include "displayfull.h"

#define PGM 0
#define SK 1
#define H 200
#define W 200

enum opValues{
  DX = 0,
  DY = 1,
  TOOL = 2,
  DATA = 3
};

enum toolValues{
  NONE = 0,
  LINE = 1,
  BLOCK = 2,
  COLOUR = 3,
  TARGETX = 4,
  TARGETY = 5
};

typedef struct state { 
  int x, y, tx, ty; 
  unsigned char tool, max, grey, image[H][W]; 
  unsigned int data; 
} state;

// prints error message to stderr then exits program
void error(char* msg){
  fprintf(stderr, "%s",msg);
  exit(1);
}

// creates state on heap and initialises values
state *newState() {
  state *s = malloc(sizeof(state));
  s->x = 0;
  s->y = 0;
  s->tx = 0;
  s->ty = 0;
  s->tool = LINE;
  s->max = 0;
  s->data = 0;
  s->grey = 0xFF;
  for (int i = 0; i < H; i++){
    for (int j = 0; j < W; j++){
      s->image[i][j] = 0;
    }
  }
  return s;
}

// frees state one used
void freeState(state *s){
  free(s);
}

// sets greyscale value based by weighting the rgb values by luminosity
// when they are not equal to each other otherwise it takes the value of one of the bytes
void greyVal(state* s){
  s->grey = 0;
  s->data = s->data >> 8; // remove opacity
  unsigned int mask = 0xFF;
  double weighting[] = {0.299, 0.587, 0.114};
  double temp = 0;
  for (int i = 2; i >= 0; i--){
    unsigned char byte = s->data & mask;
    temp += ((double) byte) * weighting[i];
    s->data = s->data >> 8;
  }
  s->grey = round(temp);

  if(s->grey > s->max) s->max = s->grey;  
}
 
// handles operations when opcode = tool
void tool(int operand, state* s){
  switch(operand){
    case NONE:
      s->tool = NONE;
      break;
    case LINE:
      s->tool = LINE;
      break;
    case BLOCK:
      s->tool = BLOCK;
      s->x = s->tx;
      s->y = s->ty;
      break;
    case COLOUR:
      greyVal(s);
      break;
    case TARGETX:
      s->tx = s->data;
      break;
    case TARGETY:
      s->ty = s->data;
      break;
  }
  s->data = 0;
}

// swaps values to make sure looping condition is not negative
void swap(int* a, int* b){
  int temp = *a;
  *a = *b;
  *b= temp;
}

// writes vertical line into pgm file
void vertical(state* s, int dy){
  bool swapped = false;
  if (dy < 0){
    dy *= -1;
    swap(&(s->y),&(s->ty));
    swapped = true;
  }
  for(int i = s->y; i <= s->ty; i++){
    s->image[i][s->x] = s->grey;
  }
  if(swapped) swap(&(s->y),&(s->ty));
}

// writes horizontal line into pgm file
void horizontal(state* s, int dx){
  bool swapped = false;
  if (dx < 0){
    dx *= -1;
    swap(&(s->x),&(s->tx));
    swapped = true;
  }
  for(int i = s->x; i <= s->tx; i++){
    s->image[s->y][i] = s->grey;
  }
  if (swapped) swap(&(s->x),&(s->tx));
}


// writes diagonal line into pgm file
void diagonal(state* s, int dx, int dy){
  int xInc = 1;
  int yInc = 1;
  if(dx < 0) xInc = -1;
  if(dy < 0) yInc = -1;
  int i = 0;
  while(i < abs(dx)){
    int xIndex = s->x + xInc * i;
    int yIndex = s->y + yInc * i;
    s->image[yIndex][xIndex] = s->grey;
    i++;
  }
}

//writes an sk line into pgm file
int writeLine(state* s){
  int forTest;
  int dx = s->tx - s->x;
  int dy = s->ty - s->y;
  if(dx == 0){
    vertical(s, dy);
    forTest = 0;
  }
  else if(dy == 0){
    horizontal(s, dx);
    forTest = 1;
  }
  else{
    diagonal(s, dx, dy);
    forTest = 2;
  }

  return forTest;
}

// writes sk block into pgm file
void writeBlock(state* s){
  int width = s->tx, height = s->ty; 
  for (int i = s->y; i < height; i++){
    for (int j = s->x; j < width; j++){
      s->image[i][j] = s->grey;
    }
  }
}

// handles operations when opcode = data
void data(int operand, state* s){
  int shift = 6;
  int mask = 0x3F;
  s->data = (s->data << shift) | (operand & mask);
}

// handles operations when opcode = dx
void dx(int operand, state* s){
  s->tx = s->tx + operand;
  if(s->tool == NONE) s->x = s->tx;
}

// handles operations when opcode = dy
void dy(int operand, state* s){
  s->ty = s->ty + operand;
  if (s->tool == LINE){
    writeLine(s);
    s->x = s->tx;
    s->y = s->ty;
  }
  else if (s->tool == BLOCK){
    writeBlock(s);
  }
  else if(s->tool == NONE) s->y = s->ty;
}

// gets leftmost two bits which represent the opcode
int getOpcode(unsigned char byte){
  const int shift = 6;
  return byte >> shift;
}

//gets first six bits which represents operand and sets sign based on twos complement
int getOperand(unsigned char byte){
  const unsigned char checkSign = 0x20, negativeMask = 0xC0, positiveMask = 0x3F;
  if ((byte | checkSign) == byte) byte = byte | negativeMask;
  else byte = byte & positiveMask;
  return (char) byte;
}

// executes current byte
void obey(state* s, unsigned char byte){
  int opcode = getOpcode(byte), operand = getOperand(byte);

  switch(opcode){
    case DX:
      dx(operand, s);
      break;
    case DY:
      dy(operand, s);
      break;
    case TOOL:
      tool(operand, s);
      break;
    case DATA:
      data(operand, s);
      break;
  }
}

// reads each byte then executes it. Also writes header and image into file
void createPgm(FILE* read, char* newName){
  state *s = newState();
  FILE* write = fopen(newName, "wb");
  unsigned char byte = fgetc(read);
  while(!feof(read)){
    obey(s, byte);
    byte = fgetc(read);
  }
  fprintf(write,"P5 %d %d %d\n", H, W, 255);
  //fwrite(s->image, 1, H*W, write);
  for(int i = 0; i < H; i++){
    for(int j = 0; j < W; j++){
      fputc(s->image[i][j],write);
    }
  }
  fflush(write);
  fclose(write);
  freeState(s);
}

const int INITIAL_CAPACITY = 100;
const int GROWTH_RATE = 5;
const int FAILURE_CODE = 1;

typedef unsigned char item;

typedef struct list{
  int length;
  int capacity;
  item* items;
} list;

// creates new list
list* newList(){
  list* skData = malloc(sizeof(list));
  skData->items = malloc(INITIAL_CAPACITY * sizeof(item));
  skData->length = 0;
  skData->capacity = INITIAL_CAPACITY;
  return skData;
}

// when the items array is full it will expand by a scale factor of GROWTH_RATE
void expand(list* skData){
  skData->capacity = skData->capacity * GROWTH_RATE;
  skData->items = realloc(skData->items, skData->capacity * sizeof(item));
}

// adds item to the list
void add(list* skData, item val){
  if(skData->length >= skData->capacity) expand(skData);
  skData->items[skData->length] = val;
  skData->length++;
}

// writes the data in the list into the sk file
void writeList(FILE* write, list* skData){
  for (int i = 0; i < skData->length; i++){
    fputc(skData->items[i], write);
  } 
}

//frees data stored on heap
void freeList(list* skData){
  free(skData->items);
  free(skData);
}

// writes the colour with two data commands
unsigned long writeColour(list* skData, unsigned char val){    
  unsigned char byte = 0;
  unsigned int colour = 0;
  unsigned long forTest = 0;
  const unsigned char COLOUR = 0x83, dataMask = 0xC0, opacity = 0xFF;
  for(int i = 0; i < 3; i++){ // for each rgb value
    colour = colour | val;
    colour = colour << 8;
  }
  colour = colour | opacity;
  
  int shift = 30;
  for (int i = 0; i < 6; i++){
    byte = ((unsigned char) (colour >> shift)) | dataMask;
    add(skData,byte);
    forTest = (forTest << 8) | byte;
    shift -= 6;
  }
  byte = COLOUR;
  add(skData,byte);
  return forTest;
}

// moving on to the next line in the pgm file for conversion
void incrementDown(list* skData){
  const int DATA0 = 0xC0, TX = 0x84, DX0 = 0, DY1 = 0X41;
  add(skData,DATA0);
  add(skData,TX);
  add(skData,DX0);
  add(skData,DY1);
}

// uses tool=targetX if value is too large to fit in one dx command
void setBigX(list* skData, int tx){
  const unsigned char DATA = 0xC0, TX = 0x84;
  unsigned char byte = 0;
    int shift = 6;
    for (int i = 0; i < 2; i++){
      byte = (tx >> shift) | DATA;
      shift -= 6;
      add(skData,byte);
    }
    add(skData,TX);
}

// moving across in pgm file for conversion
void incrementAcross(list* skData, int xIndex, unsigned char val, int length){
  const unsigned char BLOCK = 0x82, NONE = 0x80, DY = 0x40;
  writeColour(skData, val);
  int maxVal = pow(2,5);
  add(skData, BLOCK);
  int tx = xIndex + length;
  if(length < maxVal){
    add(skData, (unsigned char) length);
  }
  else{
    setBigX(skData, tx);
  }
  val = DY+1;
  add(skData,val);
  add(skData,NONE);
  val = DY | 0x3F;
  add(skData,val);
}

// reads next colour from pgm file
unsigned char readColour(FILE* read, int max, double scaleFactor){
  unsigned int temp = fgetc(read);
  if(max > 255) temp = (temp << 8) + fgetc(read);
  temp = (double) temp * scaleFactor;
  return (unsigned char) temp;
}

// reads greyscale values and puts them into a 2d array
void getPgm(FILE* read, int height, int width, unsigned char pgm[height][width], int max, double scaleFactor){
  for(int i = 0; i < H; i++){
    for(int j = 0; j < W; j++){
      unsigned int temp = fgetc(read);
      if(max > 255){
        unsigned char byte = fgetc(read);
        int shift = 8;
        temp = (temp << shift) | byte;
        temp = (unsigned int) (((double) temp) * scaleFactor);
      }
      pgm[i][j] = (unsigned char) temp;
    } 
  }
}

// calculates the scale factor for mapping pgm colours to sk colours
double getScaleFactor(int max){
  unsigned char white = 0xFF;
  double scaleFactor = 0; 
  if(max != 0) scaleFactor = (double) white / (double) max;
  return scaleFactor;
}

void rle(list* skData, int height, int width, unsigned char pgm[height][width]){
  for (int i = 0; i < H; i++){
    for(int j = 0; j < W; j++){
      int index = j;
      int pixels = 1;
      while(j < W-1 && pgm[i][j] == pgm[i][j+1]){
        pixels++;
        j++;
      }
      incrementAcross(skData,index,pgm[i][index],pixels);
    }
    incrementDown(skData);
  }
}

// takes a pgm file and outputs an sk file
void createSk(FILE* read, char* newName, int max, int seek){
  FILE* write = fopen(newName, "wb");
  list* skData = newList();
  fseek(read, seek, SEEK_SET);
  double scaleFactor = getScaleFactor(max);
  unsigned char pgm[H][W];
  getPgm(read, H, W, pgm, max, scaleFactor);
  rle(skData,H,W,pgm);
  writeList(write, skData);
  freeList(skData);
  fflush(write);
  fclose(write);
}

//reads n bytes and compares
void compare(FILE* read, int n, unsigned int cmp, unsigned int* bytes, int* seek){
  for (int i = 0; i < n; i++){
    *bytes = *bytes << 8;
    *bytes += fgetc(read);
    *seek = *seek + 1;
  }
    
  if(*bytes != cmp) error("header error\n");
}

// checks for whitespace. many is set to true if more than one is allowed.
void whitespace(FILE* read, bool many, unsigned int* bytes, int* seek){
  *bytes = fgetc(read);
  *seek = *seek + 1;
  if(!isspace(*bytes)) error("missing whitespace\n");
  *bytes = fgetc(read);
  if(many && isspace(*bytes)){
    while(isspace(*bytes)){
      *bytes = fgetc(read);
    }
  }
  else if(!many && isspace(*bytes)) error("Only one whitespace allowed here\n");
}

// getting the maxiumum grey value and dealing with single whitespace
int getMax(FILE* read, unsigned int* bytes, int* seek){
  *bytes = *bytes -'0';
  for (int i = 0; i < 2; i++){
    *bytes = *bytes *10;
    *bytes += fgetc(read) - '0';
    *seek = *seek + 1;
  }
  int max = *bytes;
  unsigned char byte = fgetc(read);
  *seek = *seek + 1;
  if(!isspace(byte)){
    *bytes = *bytes << 8;
    *bytes += fgetc(read);
    *seek = *seek + 1;
    if(*bytes >= 65536) error("maximum value error\n");
      max = *bytes;
    }

  return max;
}

// checks if the pgm file is valid by checking the header
// and that the grey values are less than the maximum
int validPgm(FILE* read, int* seek){
  unsigned int max = 0;
  unsigned int bytes = 0;
  compare(read, 2, ('P' << 8) + '5', &bytes, seek);
  whitespace(read, true, &bytes, seek);
  compare(read, 2, ('2' << 16) + ('0' << 8) + '0', &bytes, seek);
  whitespace(read, true, &bytes, seek);
  compare(read, 2, ('2' << 16) + ('0' << 8) + '0', &bytes, seek);
  whitespace(read, true, &bytes, seek);
  max = getMax(read, &bytes, seek);
  for(int i = 0; i < H*W; i++){
    unsigned int val = 0;
    val = val << 8;
    val += fgetc(read);
    if(max > 255){
      val = val << 8;
      val += fgetc(read);
    }
    if(val > max) error("value greater than max exists\n");
  }
  return max;
}

// in case there is a ./ at the beginning
char* removeBeginning(char name[]){
  int length = strlen(name);
  
  for (int i = 0; i < length-2; i++){
    name[i] = name[i+2];
  }
  name[length-2] = '\0';

  return name;
}

// remove current file type
char* removeEnd(char name[]){
  int length = strlen(name);
  while(name[length] != '.'){
    length--;
  }
  name[length] = '.';
  length++;
  name[length] = '\0';

  return name;
}

// creating the new file name
char* getNewName(char current[], int type){
  if(current[0] == '.' && current[1] == '/') removeBeginning(current);

  removeEnd(current);

    if(type == PGM){
      char pgm[]= "pgm";
      strcat(current, pgm);
    }
    else{
      char sk[] = "sk";
      strcat(current, sk);
    }
  return current;
}

// sees if file user entered is a pgm or sk
int fileEnding(char name[]){
  int len = strlen(name);
  int retVal = -1;
  if(len < 4) error("invalid file type\n");
  if(name[len-3] == '.' && name[len-2] == 's' && name[len-1] == 'k') retVal = SK;
  else{
    if(len < 5) error("invalid file type\n");
    if(name[len-4]=='.' && name[len-3]=='p' && name[len-2]=='g' && name[len-1]=='m') retVal = PGM;
  }

  return retVal;
}

void assert(int line, bool check){
  if(!check){
    printf("error on line %d\n",line);
    exit(1);
  }
}

void testType(){
  assert(__LINE__, fileEnding("hello.pgm") == PGM);
  assert(__LINE__, fileEnding("./hello.pgm") == PGM);
  assert(__LINE__, fileEnding("hello.sk") == SK);
  assert(__LINE__, fileEnding("./hello.sk") == SK);
}

void testNewName(){
  char tst1[] = "hello.pgm", tst2[] = "./hello.pgm", tst3[11] = "hello.sk", tst4[11] = "./hello.sk";
  assert(__LINE__, strcmp(getNewName(tst1, SK), "hello.sk") == 0);
  assert(__LINE__, strcmp(getNewName(tst2, SK), "hello.sk") == 0);
  assert(__LINE__, strcmp(getNewName(tst3, PGM), "hello.pgm") == 0);
  assert(__LINE__, strcmp(getNewName(tst4, PGM), "hello.pgm") == 0);
}

void testRemoveBeginning(){
  char tst1[] = "./hello.sk", tst2[] = "./hello.pgm";
  assert(__LINE__, strcmp(removeBeginning(tst1),"hello.sk") == 0);
  assert(__LINE__, strcmp(removeBeginning(tst2),"hello.pgm") == 0);
}

void testRemoveEnd(){
  char tst[] = "hello.pgm", tst2[] = "hello.sk";
  assert(__LINE__, strcmp(removeEnd(tst),"hello.") == 0);
  assert(__LINE__, strcmp(removeEnd(tst2),"hello.") == 0);
}

void testScaleFactor(){
  assert(__LINE__, getScaleFactor(0xFF)== 1);
  assert(__LINE__, getScaleFactor(0)== 0);
  assert(__LINE__, (getScaleFactor(0xFFFF) - 0.003891) < 0.0000005);
}


void testWriteColour(){
  list* skData = newList();
  assert(__LINE__, writeColour(skData,0) == 0xC0C0C0C0C3FF);
  assert(__LINE__, writeColour(skData,0xFF) == 0xC3FFFFFFFFFF);
  assert(__LINE__, writeColour(skData,0xE2) == 0XC3E2F8EECBFF);
  freeList(skData);
}

void testExpandList(){
  list* skData = newList();
  assert(__LINE__, skData->capacity == 100);
  expand(skData);
  assert(__LINE__, skData->capacity == 500);
  expand(skData);
  assert(__LINE__, skData->capacity == 2500);
  expand(skData);
  assert(__LINE__, skData->capacity == 12500);
  freeList(skData);
}

void testAddToList(){
  list* skData = newList();
  assert(__LINE__,  skData->capacity == 100);
  for (int i = 0; i < 101; i++){
    add(skData,i);
  }
  assert(__LINE__, skData->length == 101);
  assert(__LINE__, skData->capacity == 500);
  for (int i = 0; i < 101; i++){
    assert(__LINE__,skData->items[i]==i);
  }
  freeList(skData);
}

void testOperand(){
  assert(__LINE__, getOperand(0xFF) == -1);
  assert(__LINE__, getOperand(0xB9) == -7);
  assert(__LINE__, getOperand(0x59) == 25);
  assert(__LINE__, getOperand(0x00) == 0);
}

void testOpcode(){
  assert(__LINE__, getOpcode(0xFF) == 3);
  assert(__LINE__, getOpcode(0xB9) == 2);
  assert(__LINE__, getOpcode(0x79) == 1);
  assert(__LINE__, getOpcode(0x00) == 0);
}

void testDy(){
  state* s = newState();
  s->tool = NONE;
  dy(5,s);
  assert(__LINE__, s->ty == 5 && s->y == 5);
  dy(0,s);
  assert(__LINE__, s->ty == 5 && s->y ==5);
  s->x = 4, s->tx = 3, s->tool = LINE;
  dy(5,s);
  assert(__LINE__, s->x == 3 && s->y == 10 && s->ty == 10);
  free(s);
}

void testDx(){
  state *s = newState();
  dx(4,s);
  assert(__LINE__, s->tx == 4 && s->x == 0);
  s->tool = NONE;
  dx(0,s);
  assert(__LINE__, s->tx == 4 && s->x == 4);
  free(s);
}

void testData(){
  state *s = newState();
  data(0xC0,s);
  assert(__LINE__, s->data == 0);
  data(0xB2,s);
  assert(__LINE__, s->data == 0x32);
  data(0xFF,s);
  assert(__LINE__, s->data == 0xCBF);
  free(s);
}

void testSwap(){
  int a = 5, b = 6;
  swap(&a, &b);
  assert(__LINE__, a == 6 && b == 5);
  swap(&b, &a);
  assert(__LINE__, a == 5 && b == 6);
}

void testLine(){
  state *s = newState();
  int type; // 0 for vertical, 1 for horizontal, 2 for diagonal
  s->ty = 4;
  type = writeLine(s);
  assert(__LINE__, type == 0);
  s->tx = 4;
  type = writeLine(s);
  assert(__LINE__, type == 2);
  s->ty = 0;
  type = writeLine(s);
  assert(__LINE__, type == 1);
  free(s);
}

void testGreyVal(){
  state *s = newState();
  s->data = 0xFFFFFFFF;
  greyVal(s);
  assert(__LINE__, s->grey == 0xFF);
  s->data = 0x000000FF;
  greyVal(s);
  assert(__LINE__, s->grey == 0x0);
  s->data = 0x0A0A0AFF;
  greyVal(s);
  assert(__LINE__, s->grey == 0x0A);
  s->data = 0xFF0000FF;
  greyVal(s);
  assert(__LINE__, s->grey == 76);
  s->data = 0x00FF00FF;
  greyVal(s);
  assert(__LINE__, s->grey == 150);
  s->data = 0x34B1D8FF;
  greyVal(s);
  assert(__LINE__, s->grey == 144);
  free(s);
}

void test(){
  testType(); // pgm to sk
  testRemoveBeginning();
  testRemoveEnd();
  testNewName();
  testScaleFactor();
  testWriteColour();
  testExpandList();
  testAddToList();
  testOperand(); // sk to pgm
  testOpcode();
  testDy(); 
  testDx();
  testData();
  testSwap();
  testLine();
  testGreyVal();
  printf("all tests passed\n");
}

int main(int n, char* args[]){
  if(n == 1) test();
  else if(n == 2){
    int type = fileEnding(args[1]);
    FILE* read = fopen(args[1], "rb");
    if(read == NULL) error("File does not exist\n");
    char* newName = getNewName(args[1], !type);
    if(type==PGM){
      int seek = 0;
      int max = validPgm(read, &seek);
      if(max > 255) seek+=5;
      else seek += 3;
      createSk(read, newName, max, seek);
    }
    else if(type == SK){
      createPgm(read, newName);
    }
    else error("invalid file type\n");
    fclose(read);
    printf("File %s has been written.\n", newName);
  }
  else{
    error("Too many arguements\n");
  }
  return 0;
}
