#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_NUM_BUF 20

class Token{
public:
  Token(char *str){
    m_token = str;
    m_pToken = m_token ;
  }
  bool complete(){
    return *m_pToken == '\0' ;
  }
  bool add(char c){
    m_pToken = (*m_pToken == c)?m_pToken+1:m_token ;
    return complete() ;
  }
  
private:
  char *m_token ;
  char *m_pToken ;
};

unsigned char hexToByte(char *szHex)
{
  // Check first 2 bytes are '0x'
  if (szHex[0] != '0' || szHex[1] != 'x'){
    fprintf(stderr, "Cannot read the bytes in the file. File corrupt or not XBM\n") ;
    exit(1) ;
  }
  int mult = 1 ;
  unsigned char ret = 0 ;
  while (mult >= 0){
    char val = 0 ;
    unsigned char c = szHex[3-mult] ;
    if (c >= '0' && c <= '9') val = c - '0' ;
    else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') val = c - 'a' + 10 ;
    else{
      fprintf(stderr, "Array doesn't contain hex values for image\n") ;
      exit (1) ;
    }
    ret = ret + (val << mult*4) ;
    mult-- ;
  }

  return ret ;
}

int main (int argc, char **argv)
{
  if (argc < 2 || argc > 3){
    printf ("Usage: xbm2bin input.xbm [out.bin]\n\tstdout will be written to if out.bin is omitted\n") ;
    return 0;
  }

  FILE *fout = stdout ;
  FILE *f = fopen(argv[1], "r") ;
  if (!f){
    fprintf(stderr, "Cannot read input file %s\n", argv[1]) ;
    return 1 ;
  }
  if (argc == 3){
    fout = fopen(argv[2], "w+") ;
    if (!fout){
      fprintf(stderr, "Cannot open %s for writing\n", argv[2]) ;
      return 1 ;
    }
  }

  Token defineToken((char*)"#define") ;
  Token staticToken((char*)"static") ;
  Token unsignedToken((char*)"unsigned") ;
  Token charToken((char*)"char") ;
  Token widthToken((char*)"width") ;
  Token heightToken((char*)"height") ;

  char c ;
  int nBuffRead = 0 ;
  bool bParamRead = false ;
  bool bReadWidth = false ;
  bool bReadHeight = false ;
  char numBuf[MAX_NUM_BUF] ;
  int numBufIndex = 0 ;

  uint32_t nHeight = 0;
  uint32_t nWidth = 0 ;
  unsigned char *imgBuf = NULL ;
  uint32_t iBuff = 0;
  uint32_t buff_size = 0;
  
  char hexBuf[5] ;
  int hexIndex = 0 ;
  
  while (fread(&c, 1, 1, f)){
    switch(c){
    case '\0':
      fprintf(stderr, "NULL character in file, could be binary. Terminating\n") ;
      return 1 ; // Shouldn't have a null in the stream of characters
    case ';':
      // end of something. Reset everything
      bParamRead = false ;
      bReadWidth = false ;
      bReadHeight = false ;
      nBuffRead = 0 ;
      if (!(bReadHeight || bReadWidth)) break ;
    case ' ' :
    case '\n':
    case '\r':
    case '\t':
      if (widthToken.complete() && bParamRead){ bReadWidth = true ; }
      if (heightToken.complete() && bParamRead){ bReadHeight = true ; }      

      // Terminate the number for width and height
      numBuf[numBufIndex] = '\0' ;
      if (bReadHeight && numBufIndex > 0){ nHeight = atoi(numBuf) ; }
      if (bReadWidth && numBufIndex > 0){ nWidth = atoi(numBuf) ;}
      numBufIndex = 0 ; // Reset the number buffer

      break ; // token dividers
    case '=':
      if (nBuffRead == 3){ nBuffRead = 4; break ;}
    case '{':
      if (nBuffRead == 4){
	if (nHeight <= 0 || nWidth <= 0){
	  fprintf(stderr, "Reading the buffer but height and width are not known\n") ;
	  return 1 ;
	}
	buff_size = (nWidth/8 + (nWidth%8?1:0)) * nHeight ;
	imgBuf = new unsigned char[buff_size] ;
	if (!imgBuf){
	  fprintf(stderr, "Memory allocation error\n") ;
	  return 1 ;
	}
	iBuff = 0; // Reset buffer index
	nBuffRead = 5 ; // into byte reading state
	break ;
      } 
    case '}':
      nBuffRead = 0 ;
    case ',':
      if (nBuffRead == 5){
	hexIndex = 0 ;
	break ;
      }
    default:
      // ASCII chars
      if (nBuffRead == 5){
	hexBuf[hexIndex++] = c ;
	if (hexIndex >= 4){
	  hexBuf[4] = '\0' ;
	  hexIndex = 0;
	  // Write byte to buffer
	  imgBuf[iBuff++] = hexToByte(hexBuf) ;
	  if (iBuff > buff_size){
	    fprintf(stderr,"Too many bytes in XBM file to fit into image %u x %u (%u bytes)\n", nWidth, nHeight, buff_size) ;
	    return 1 ;
	  }
	}
      }
      
      if (!(c >= '0' && c <= '9')){
	// not a number
	bReadWidth = false ;
	bReadHeight = false ;
      }
      
      if (bReadWidth || bReadHeight){
	numBuf[numBufIndex++] = c ;
	if (numBufIndex >= MAX_NUM_BUF - 1){
	  fprintf(stderr, "Buffer overflow for width/height value in file\n") ;
	  return 1 ;
	}
      }

      // Feed the state machines
      widthToken.add(c) ;
      heightToken.add(c) ;
      if (defineToken.add(c)){ bParamRead = true ;}
      if (staticToken.add(c)) nBuffRead = 1 ;
      if (unsignedToken.add(c) && nBuffRead == 1) nBuffRead = 2 ;
      if (charToken.add(c) && nBuffRead == 2) nBuffRead = 3;
    }
    
  }

  // Stick out to stdout for now
  fwrite(&nWidth, sizeof(uint32_t), 1, fout) ;
  fwrite(&nHeight, sizeof(uint32_t), 1, fout) ;
  fwrite(imgBuf, buff_size, 1, fout) ;

  fclose(f) ;
  fclose(fout) ;
  
  return 0 ;
}
