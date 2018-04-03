#include "Pokitto.h"
#include "gfx.h"
#include "SDFileSystem.h"
#include "easing.h"

SDFileSystem sd(/*MOSI*/P0_9, /*MISO*/P0_8, /*SCK*/P0_6, /*CS*/P0_7, /*Mountpoint*/"sd");

Pokitto::Core game;

/**************************************************************************/
#define HELD 0
#define NEW 1
#define RELEASE 2
byte CompletePad, ExPad, TempPad, myPad;
bool _A[3], _B[3], _C[3], _Up[3], _Down[3], _Left[3], _Right[3];

DigitalIn _aPin(P1_9);
DigitalIn _bPin(P1_4);
DigitalIn _cPin(P1_10);
DigitalIn _upPin(P1_13);
DigitalIn _downPin(P1_3);
DigitalIn _leftPin(P1_25);
DigitalIn _rightPin(P1_7);

void UPDATEPAD(int pad, int var) {
  _C[pad] = (var >> 1)&1;
  _B[pad] = (var >> 2)&1;
  _A[pad] = (var >> 3)&1;
  _Down[pad] = (var >> 4)&1;
  _Left[pad] = (var >> 5)&1;
  _Right[pad] = (var >> 6)&1;
  _Up[pad] = (var >> 7)&1;
}

void UpdatePad(int joy_code){
  ExPad = CompletePad;
  CompletePad = joy_code;
  UPDATEPAD(HELD, CompletePad); // held
  UPDATEPAD(RELEASE, (ExPad & (~CompletePad))); // released
  UPDATEPAD(NEW, (CompletePad & (~ExPad))); // newpress
}

byte updateButtons(byte var){
   var = 0;
   if (_cPin) var |= (1<<1);
   if (_bPin) var |= (1<<2);
   if (_aPin) var |= (1<<3); // P1_9 = A
   if (_downPin) var |= (1<<4);
   if (_leftPin) var |= (1<<5);
   if (_rightPin) var |= (1<<6);
   if (_upPin) var |= (1<<7);

   return var;
}


bool saveBMP(char* filename){
// no longer used
char w=110, h=88;

FILE *f;
int filesize = 54 + 3*w*h;  //w is your image width, h is image height, both int

unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
unsigned char bmppad[3] = {0,0,0};

bmpfileheader[ 2] = (unsigned char)(filesize    );
bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
bmpfileheader[ 4] = (unsigned char)(filesize>>16);
bmpfileheader[ 5] = (unsigned char)(filesize>>24);

bmpinfoheader[ 4] = (unsigned char)(       w    );
bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
bmpinfoheader[ 6] = (unsigned char)(       w>>16);
bmpinfoheader[ 7] = (unsigned char)(       w>>24);
bmpinfoheader[ 8] = (unsigned char)(       h    );
bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
bmpinfoheader[10] = (unsigned char)(       h>>16);
bmpinfoheader[11] = (unsigned char)(       h>>24);

f = fopen(filename,"wb");
fwrite(bmpfileheader,1,14,f);
fwrite(bmpinfoheader,1,40,f);


for(int y=0; y<h; y++){
    for(int x=0; x<w; x++){
        int pix = game.display.getPixel(x,h-y-1);
        int color = game.display.palette[pix];
        char r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
        char g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
        char b = (((color & 0x1F) * 527) + 23) >> 6;
        fwrite(&b , 1 , sizeof(b) , f);
        fwrite(&g , 1 , sizeof(g) , f);
        fwrite(&r , 1 , sizeof(r) , f);
    }
    fwrite(bmppad,1,(4-(w*3)%4)%4,f);
}

fclose(f);
}

/**************************************************************************/
#define MAX_CARDS 51
#define CARD_SPACE 3
#define EXTRA_SPACE 3
#define DEALFRAMES 8
// arrays for columns...
signed char pile[13][25]; // 7 columns, max 20 cards (7 face down, 13 face up)
signed char holding[25];
char numHold=0;
signed char pileLength[13];
char deck1[52], deck2[52];
bool faceUp[52]; // card facing up or down
char cardX[52], cardY[52]; // for animating
bool cardZ[52]; // is card dealt yet?
uint8_t tempBG[178]; // bg mask for single card

char cardOrder[53];
char gameMode;
char hX,hY=0;
signed char curCol; // current column number
char curRow; // number down the column
signed char oldCol; // current column number
char oldRow; // number down the column
signed char curCard=-1;
char suits[4]={0,1,2,3}; // order these black,red,black,red
bool useDeck=0;
long frameNumber;
int menuItem;
bool draw3=0;



void drawFont2(int x1, int y1, int wide, int high, int tile, int col, const uint8_t *gfx){
    uint8_t pix;
    uint8_t pic;

    int transparentColor = 0;//game.display.getInvisibleColor();
    for(int y=0; y<high; y++){
        pic = gfx[y+2];
        pix = (pic >> 7)&1; if(pix != transparentColor) game.display.drawPixel(  x1,y1+y,pix+col-1);
        pix = (pic >> 6)&1; if(pix != transparentColor) game.display.drawPixel(1+x1,y1+y,pix+col-1);
        pix = (pic >> 5)&1; if(pix != transparentColor) game.display.drawPixel(2+x1,y1+y,pix+col-1);
        pix = (pic >> 4)&1; if(pix != transparentColor) game.display.drawPixel(3+x1,y1+y,pix+col-1);
        //pix = (pic >> 3)&1; if(pix != transparentColor) game.display.drawPixel(4+x1,y1+y,pix+col-1);
        //pix = (pic >> 2)&1; if(pix != transparentColor) game.display.drawPixel(5+x1,y1+y,pix+col-1);
        //pix = (pic >> 1)&1; if(pix != transparentColor) game.display.drawPixel(6+x1,y1+y,pix+col-1);
        //pix = (pic)&1;      if(pix != transparentColor) game.display.drawPixel(7+x1,y1+y,pix+col-1);
    }
}

// print text
void print(uint16_t x, uint16_t y, const char* text, uint8_t Col){
    for(uint8_t t = 0; t < strlen(text); t++){
        uint8_t character = text[t]-32;
        drawFont2(x, y, 4, 6, character, Col, &font1[character][0]);
        x += 4;
    }
}

char findLength(char x){
    int pL=-1;
    for(int y=0; y<25; y++){ // max length should be 7 face down, 13 face up
        if(pile[x][y] == -1 && pL==-1){
            return y;
        }
    }
    return 0;
}


void shuffle(){
    // arrange deck before shuffle
    for(int t=0; t<=MAX_CARDS; t++){
        deck1[t]=t; // order cards by number
        faceUp[t]=0; // make cards face down
    }
    // shuffle deck
    for(int t=0; t<=MAX_CARDS; t++){
        int temp = random(MAX_CARDS);
        int tmp = deck1[temp];
        deck1[temp]=deck1[t];
        deck1[t]=tmp;
    }
}


void updateGameScreen(){
    // clear screen col 12
    game.display.setColor(hand[2]); // background color from top left cursor pixel
    game.display.fillRectangle(0,0,110,88);

    // deck
    game.display.drawBitmap(10,1,cards[64]);
    game.display.drawBitmap(23,1,cards[64]);

    // playfield
    for(int x=0; x<7; x++){
        char addMe=0;
        for(int y=0; y<25; y++){ // max length should be 7 face down, 13 face up
            signed int thisCard = pile[x][y];

            if(y>1){
                if(faceUp[pile[x][y-1]] != faceUp[pile[x][y-2]]){
                    addMe=+EXTRA_SPACE;
                }
            }
            if(y==1){
                if(faceUp[pile[x][y-1]]){
                    addMe=+EXTRA_SPACE;
                }
            }

            if(x==curCol && y==curRow+1){
                addMe=+EXTRA_SPACE+EXTRA_SPACE;
            }


            if(thisCard >= 0 && thisCard<=51){

                cardX[thisCard]=10+x*13;
                cardY[thisCard]=addMe+17+y*CARD_SPACE;
                if(faceUp[thisCard]==1){
                    game.display.drawBitmap(10+x*13,addMe+17+y*CARD_SPACE,cards[thisCard]);
                }else{
                    game.display.drawBitmap(10+x*13,addMe+17+y*CARD_SPACE,cards[52]);
                }
            }else{
                if(pileLength[x]==0 && y==0){
                    game.display.drawBitmap(10+x*13,addMe+17+y*CARD_SPACE,cards[64]); // empty box
                }
            }
        }
    }

    // draw the remaining deck
    if(pileLength[7]){
        game.display.drawBitmap(10,1,cards[52]); // show top card for testing
    }
    if(pileLength[8]){
        if(draw3==0){
            game.display.drawBitmap(23,1,cards[pile[8][pileLength[8]-1]]); // show top card for testing
        }else{
            if(pileLength[8]-2){game.display.drawBitmap(23,1,cards[pile[8][pileLength[8]-3]]);}
            if(pileLength[8]-1){game.display.drawBitmap(28,1,cards[pile[8][pileLength[8]-2]]);}
            if(pileLength[8]){game.display.drawBitmap(33,1,cards[pile[8][pileLength[8]-1]]);}
        }
    }


    // draw the 4 piles
    for(int t=0; t<4; t++){
        if(pileLength[9+t]){
            game.display.drawBitmap(49+t*13,1,cards[pile[9+t][pileLength[9+t]-1]]);
        }else{
            game.display.drawBitmap(49+t*13,1,cards[60+t]);
        }
    }

    if(numHold==0){
        //if(frameNumber&1){
            game.display.drawBitmap(hX,hY,hand);
        //}
    }else{
        if(curCard!=-1){
            char addMe=0;
            for(int t=0; t<numHold; t++){
                if(t==1){addMe=CARD_SPACE;}
                game.display.drawBitmap(hX,addMe+hY+(t*CARD_SPACE),cards[holding[t]]);
            }
        }
    }


/*
    char tempText[15];
    sprintf(tempText,"%d,%d,%d,%d,%d", numHold, curCard,curCol,findLength(7),findLength(8));
    print(0,80,tempText,47);
*/
    frameNumber++;
}

void getBitmap(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t* bitmap)
{

    *bitmap++ = w;
    *bitmap++ = h;

    for(int y1=0; y1<h; y1++){ // max length should be 7 face down, 13 face up
        for(int x1=0; x1<w; x1++){
            *bitmap++ = game.display.getPixel(x+x1,y+y1);
        }
    }
}


void deal(){

    // draw the 4 piles
    for(int t=0; t<4; t++){
        game.display.drawBitmap(49+t*13,1,cards[60+t]);
    }
    // deck
    game.display.drawBitmap(10,1,cards[52]); // show top card for testing
    game.display.drawBitmap(23,1,cards[64]);


    game.display.setInvisibleColor(-1);
    game.display.persistence=1;

    frameNumber=0;
    int cX,cY;
    for(int y=0; y<7; y++){ // max length should be 7 face down, 13 face up
        for(int x=0; x<7; x++){
            signed int thisCard = pile[x][y];
            if(thisCard !=-1){
                for(int fr=0; fr<DEALFRAMES; fr++){

                    myPad = updateButtons(myPad);
                    UpdatePad(myPad);

                    if(_A[NEW]){
                        y=7; x=7;
                        break;
                    }

                    cX = 10+ easeOutQuad(fr, 1, cardX[thisCard]-10, DEALFRAMES);
                    cY = easeOutQuad(fr, 1, cardY[thisCard], DEALFRAMES);
                    getBitmap(cX,cY,11,16,tempBG);


                    if(faceUp[thisCard]==1){
                        game.display.drawBitmap(cX,cY,cards[thisCard]);
                    }else{
                        game.display.drawBitmap(cX,cY,cards[52]);
                    }

                    game.display.update();
                    game.display.drawBitmap(cX,cY,tempBG);
                }

                if(faceUp[thisCard]==1){
                    game.display.drawBitmap(cardX[thisCard],cardY[thisCard],cards[thisCard]);
                }else{
                    game.display.drawBitmap(cardX[thisCard],cardY[thisCard],cards[52]);
                }

            }
        }
    }
    game.display.setInvisibleColor(hand[2]);
    game.display.persistence=0;
}

void newGame(){
    // clear the piles
    for(int y=0; y<25; y++){
        for(int x=0; x<13; x++){
            pile[x][y]=-1; // clear all of the columns
        }
    }

    shuffle();
    int cardNum=0;
    for(int y=0; y<7; y++){
        for(int x=0; x<7; x++){
            if(y<=x){
                pile[x][y]=deck1[cardNum];
                cardX[deck1[cardNum]]=10+x*13;
                cardY[deck1[cardNum]]=17+y*CARD_SPACE;
                cardZ[deck1[cardNum]]=0;

                if(y==x){
                    faceUp[pile[x][y]]=1;
                }
                cardNum++;
            }
        }
    }

    // rest of the cards to the main pile (8)
    int t=0;
    while(cardNum<=MAX_CARDS){
            pile[7][t]=deck1[cardNum++];
            t++;
    }
    curCol=0;
    curRow=0;
    numHold=0;
    curCard=-1;
    useDeck=1;
    frameNumber=0;

    deal();
    gameMode=10;
}


void setup(){
    game.begin();
    game.display.width = 110; // half size
    game.display.height = 88;
    game.display.persistence=0;
    game.display.setFont(font3x5);
    game.display.setInvisibleColor(hand[2]);
	srand(game.getTime());
	gameMode=0;
	curCol=0;
	curRow=0;
	gameMode=0;
}






void collectCards(){

    int cX,cY;
    bool goAgain=1;

    game.display.setInvisibleColor(-1);
    game.display.persistence=1;

    while(goAgain==1){

        for(int x=0; x<13; x++){
            pileLength[x]=findLength(x);
        }


        goAgain=0;
        for(int t=0; t<7; t++){
            int s = findLength(t);
            if(s>0){
                int thisCard = pile[t][s-1];
                char suit = thisCard/13;
                char number = thisCard%13;
                if(pileLength[9+suit]==number){
                    pile[t][s-1]=-1;
                    pile[9+suit][pileLength[9+suit]]=thisCard;

                    for(int fr=0; fr<DEALFRAMES; fr++){

                        cX = easeOutQuad(fr, cardX[thisCard], (49+suit*13)-cardX[thisCard], DEALFRAMES);
                        cY = cardY[thisCard] - easeOutQuad(fr, 2, cardY[thisCard], DEALFRAMES) +1;
                        getBitmap(cX,cY,11,16,tempBG);

                        game.display.drawBitmap(cX,cY,cards[thisCard]);

                        game.display.update();
                        game.display.drawBitmap(cX,cY,tempBG);

                    }

                    game.display.drawBitmap(49+suit*13,1,cards[pile[9+suit][pileLength[9+suit]-1]]);

                    if( pileLength[9]!=12 && pileLength[10]!=12 && pileLength[11]!=12 && pileLength[12]!=12 ) { goAgain = 1;}

                    for(int x=0; x<13; x++){
                        pileLength[x]=findLength(x);
                    }
                    game.display.setInvisibleColor(hand[2]);
                    updateGameScreen();
                    game.display.setInvisibleColor(-1);

                }
            }
        }
    }
    game.display.setInvisibleColor(hand[2]);
    game.display.persistence=0;


}


void checkComplete(){
    bool complete=0;

    if(pileLength[7]==0 && pileLength[8]==0){
        if(numHold==0 && curCard==-1){
            //finished game?
            complete=1;
            for(int t=0; t<51; t++){
                if(!faceUp[t]){
                    complete = 0;
                }
            }
        }
    }

    if(complete==1){
        collectCards();
        game.display.load565Palette(&title_screen_pal[0]);
        gameMode=1; // title screen
    }
}


void checkBottoms(){
    for(int t=0; t<7; t++){
        int s = findLength(t);
        if(s>0){
            faceUp[pile[t][s-1]]=1;
        }
    }
}


void mainGame(){
            updateGameScreen();
            if(_B[NEW]){
                //saveBMP("/sd/screen.bmp");
                if(curCard==-1){ // not holding a card
                    for(int t=pileLength[curCol]; t>=0; t--){
                        if(faceUp[pile[curCol][t]]){
                            curRow=t;
                        }
                    }

                    if(curCol!=7){
                        if(faceUp[pile[curCol][curRow]]){ // if the card is face up, then grab it
                            oldCol=curCol;
                            oldRow=curRow-1;
                            curCard=pile[curCol][curRow];
                            int t=0;
                            while(curRow < pileLength[curCol]){
                                holding[t++]=pile[curCol][curRow];
                                pile[curCol][curRow++]=-1;
                                numHold++;
                            }
                            curRow = findLength(curCol)-1;
                        }else{
                            // turn card over
                            if(curRow == pileLength[curCol]-1){faceUp[pile[curCol][curRow]]=1;}
                        }
                    }

                }
            }

            if(_A[RELEASE] || _Up[RELEASE]){
                    checkComplete();
            }

            if(_A[NEW]){

                if(curCard==-1){ // not holding a card

                    if(curCol==7){
                        if(findLength(7)){
                            if(draw3==0){
                                pile[8][findLength(8)]=pile[7][findLength(7)-1];
                                pile[7][findLength(7)-1]=-1;
                                faceUp[pile[8][findLength(8)-1]]=1;
                            }else{
                                pile[8][findLength(8)]=pile[7][findLength(7)-1];
                                pile[7][findLength(7)-1]=-1;
                                faceUp[pile[8][findLength(8)-1]]=1;

                                if(findLength(7)){
                                    pile[8][findLength(8)]=pile[7][findLength(7)-1];
                                    pile[7][findLength(7)-1]=-1;
                                    faceUp[pile[8][findLength(8)-1]]=1;
                                }

                                if(findLength(7)){
                                    pile[8][findLength(8)]=pile[7][findLength(7)-1];
                                    pile[7][findLength(7)-1]=-1;
                                    faceUp[pile[8][findLength(8)-1]]=1;
                                }
                            }
                        }else{
                            int t1 = findLength(8);
                            for(int t=0; t<t1; t++){
                                pile[7][t]=pile[8][t1-t-1];
                                pile[8][t1-t-1]=-1;
                            }
                        }
                    }


                    if(curCol!=7){
                        if(faceUp[pile[curCol][curRow]]){ // if the card is face up, then grab it
                            oldCol=curCol;
                            oldRow=curRow-1;
                            curCard=pile[curCol][curRow];
                            int t=0;
                            while(curRow < findLength(curCol)){
                                holding[t++]=pile[curCol][curRow];
                                pile[curCol][curRow++]=-1;
                                numHold++;
                            }
                            curRow = findLength(curCol)-1;
                        }else{
                            // turn card over
                            if(curRow == findLength(curCol)-1){faceUp[pile[curCol][curRow]]=1;}
                        }
                    }
                }else{ // holding a card
                    // is old position? (not moved)
                    if(oldCol==curCol && oldRow==curRow){
                        for(int t=0; t<numHold; t++){
                            pile[curCol][pileLength[curCol]+t]=holding[t];
                        }
                        numHold=0; curCard=-1; curRow = findLength(curCol)-1;
                    }else{
                        // is top card different colour?
                        char myCard = holding[0]/13; // get suit
                        char placeCard = pile[curCol][pileLength[curCol]-1]/13;
                        if(myCard%2 != placeCard%2){
                            // suit is correct, now try number!
                            myCard = holding[0]%13; // get number
                            placeCard = pile[curCol][pileLength[curCol]-1]%13;
                            if(myCard==placeCard-1){
                                for(int t=0; t<numHold; t++){
                                    pile[curCol][pileLength[curCol]+t]=holding[t];
                                }
                                numHold=0; curCard=-1; curRow = findLength(curCol)-1;
                                // need to check for no card first.
                                checkBottoms();
                            }
                        }// suit?

                    // king on space?
                    if(pileLength[curCol]==0){
                        if(holding[0]%13==12){
                            for(int t=0; t<numHold; t++){
                                pile[curCol][pileLength[curCol]+t]=holding[t];
                            }
                            numHold=0; curCard=-1; curRow = findLength(curCol)-1;
                            checkBottoms();
                        }
                    }

                    }

                }

                // Ace on free pile?
                if(numHold==1){
                    // only one card
                    char suit = holding[0]/13;
                    char number = holding[0]%13;
                    if(pileLength[9+suit]==number && curCol == 9+suit){
                        pile[9+suit][pileLength[9+suit]]=holding[0];
                        numHold=0; curCard=-1; curRow = findLength(curCol)-1;
                        checkBottoms();
                    }
                }


            }

            if(_C[NEW]){
                //saveBMP("/sd/screen.bmp");
                gameMode=15;
            }

            if(_Up[NEW]){
                // not holding
                if(curCard==-1){
                    if(curRow>0 && faceUp[pile[curCol][curRow-1]]){
                        curRow--;
                    }else{
                        curCol=7;
                    }
                    if(pileLength[curCol]==0){
                        curCol=7;
                    }
                }

                // holding
                if(curCard!=-1){
                        if(numHold==1){
                            // only one card
                            char suit = holding[0]/13;
                            char number = holding[0]%13;
                            if(pileLength[9+suit]==number){
                                pile[9+suit][pileLength[9+suit]]=holding[0];
                                numHold=0; curCard=-1; curRow = findLength(curCol)-1;
                                checkBottoms();
                                //if(oldCol<7 && oldRow!=0)faceUp[pile[oldCol][oldRow]]=1; // auto turn old card
                            }
                        }
                }

            }
            if(_Down[NEW]){
                switch(curCol){
                    case 0:
                    case 1:
                        curCol+=7;
                        curRow = pileLength[curCol]-1;
                    break;
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                        curCol+=6;
                        curRow = pileLength[curCol]-1;
                    break;
                    case 7:
                    case 8:
                        curCol-=7;
                        curRow = pileLength[curCol]-1;
                    break;
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                        curCol-=6;
                        curRow = pileLength[curCol]-1;
                    break;
                }
                if(curCol>12)curCol=12;

                /*
                if(curRow<pileLength[curCol]-1){
                    if(curCard==-1){curRow++;}
                }else{
                    curCol=7;
                }
*/


            }
            if(_Left[NEW]){
                bool doneMoving=0;
                while(!doneMoving){
                    switch(--curCol){
                        case -1:
                            curCol=6;
                            break;
                        case 6:
                            curCol=12;
                            break;
                    }
                    if(pileLength[curCol]!=0 || holding[0]%13==12 || curCol==7){doneMoving=1;}
                    if(pileLength[curCol]==0 && holding[0]%13!=12 && curCol!=7){doneMoving=0;}
                }
                curRow=pileLength[curCol]-1;
            }


            if(_Right[NEW]){

                bool doneMoving=0;
                while(!doneMoving){
                    switch(++curCol){
                    case 7:
                        curCol=0;
                        break;
                    case 13:
                        curCol=7;
                        break;
                    }
                    if(pileLength[curCol]!=0 || holding[0]%13==12 || curCol==7){doneMoving=1;}
                    if(pileLength[curCol]==0 && holding[0]%13!=12 && curCol!=7){doneMoving=0;}
                }
                curRow=pileLength[curCol]-1;
            }

            // playfield
            for(int x=0; x<13; x++){
                pileLength[x]=findLength(x);
            }

            // hand cursor
            if(curCol<7){
                hX=curCol*13+13;
                hY=curRow*CARD_SPACE+26;
            }else{
                hX=(curCol-7)*13+16;
                hY=6;
                if(curCol>=9){
                    hX=(curCol-6)*13+13;
                }
            }

}


void titleScreen(){

    game.display.drawBitmap(0,0,title_screen);

    if(_A[NEW]){
        game.display.setColor(hand[2]);
        game.display.fillRectangle(0,0,110,88);
        game.display.load565Palette(&cards_pal[0]);
        newGame();

        gameMode=10;
    }

}

void completeScreen(){

    game.display.drawBitmap(6,33,welldone);
    if(_A[NEW]){
        gameMode=0;
    }

}


void saveGame(){

    // test for folder
    mkdir("/sd/pokittaire", 0777);

    FILE *file;
    file = fopen("/sd/pokittaire/save.dat", "wb");

    if(file){

        for(int t=0; t<13; t++){
                for(int s=0; s<25; s++){
                    fwrite(&pile[t][s],1,1,file);
                }
        }
        for(int t=0; t<13; t++){
            fwrite(&pileLength[t],1,1,file);
        }
        for(int t=0; t<52; t++){
            fwrite(&faceUp[t],1,1,file);
        }

    }//file

    fclose(file);

}

void loadGame(){

    // test for folder
    mkdir("/sd/pokittaire", 0777);

    FILE *file;
    file = fopen("/sd/pokittaire/save.dat", "rb");

    if(file){

        for(int t=0; t<13; t++){
                for(int s=0; s<25; s++){
                    fread(&pile[t][s],1,1,file);
                }
        }
        for(int t=0; t<13; t++){
            fread(&pileLength[t],1,1,file);
        }
        for(int t=0; t<52; t++){
            fread(&faceUp[t],1,1,file);
        }

    }

    fclose(file);

}


void saveSettings(){
    // test for folder
    mkdir("/sd/pokittaire", 0777);

    FILE *file;
    file = fopen("/sd/pokittaire/settings.dat", "wb");
    if(file){
        fwrite(&draw3,1,1,file);
    }
    fclose(file);
}

void loadSettings(){
    // test for folder
    mkdir("/sd/pokittaire", 0777);
    // read settings
    int c;
    FILE *file;
    file = fopen("/sd/pokittaire/settings.dat", "r");
    if(file){
        c = getc(file);
            draw3=0;
        if(c){
            draw3=1;
        }
        fclose(file);
    }else{
        saveSettings();
    }
}

void displayMenu(){

    updateGameScreen();

    game.display.drawBitmap(21,18,box);

    int textCol=47;
    print(29,20,"POKITTAIRE",textCol);
    print(35,30,"  LOAD",textCol);  // 0
    print(35,36,"  SAVE",textCol);  // 1

    switch(draw3){
        case 0:
            print(35,42," DEAL 3",textCol); // 2
            break;
        case 1:
            print(35,42," DEAL 1",textCol); // 2
            break;
    }
    print(35,48,"CONTINUE",textCol);// 3
    print(35,54,"NEW GAME",textCol);// 4
    print(35,60,"  QUIT",textCol);   // 5

    game.display.drawBitmap(69,31+menuItem*6,hand);
}

void menu(){

    if(_Up[NEW] && menuItem>0){
        menuItem--;
    }
    if(_Down[NEW] && menuItem<5){
        menuItem++;
    }

    if(_B[NEW] || _C[NEW]){
        // return to game
        gameMode=10;
    }

    if(_A[NEW]){
        switch(menuItem){
            case 0:
                loadGame();
                gameMode=10;
                break;
            case 1:
                saveGame();
                gameMode=10;
                break;
            case 2:
                draw3 = 1-draw3;
                saveSettings();
                break;
            case 3:
                // continue
                gameMode=10;
                break;
            case 4:
                // restart
                newGame();
                break;
            case 5:
                // quit
                gameMode=0;
                game.display.load565Palette(&title_screen_pal[0]);
                break;
        }
    }


    if(gameMode==15) displayMenu();
}


int main(){
    setup();
    loadSettings();

    int cardNum=0;

    int cx,cy=0;
    game.display.load565Palette(&title_screen_pal[0]);
    game.display.drawBitmap(0,0,title_screen);

    while (game.isRunning()) {
        if(game.update()){
            myPad = updateButtons(myPad);
            UpdatePad(myPad);

            switch (gameMode){
                case 0:
                    titleScreen();
                    break;
                case 1:
                    completeScreen();
                    break;
                case 10:
                    mainGame();
                    break;
                case 15:
                    menu();
                    break;
            }

        }
    }

    return 1;
}
