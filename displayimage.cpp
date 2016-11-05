#include "displayimage.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "jpeglib.h"
#include <math.h>

DisplayImage::DisplayImage()
{
  m_img = NULL ;
  m_width = 0 ;
  m_height = 0 ;
  m_bResourceImage = false ;
  m_memsize = 0 ;
  m_stride = 0;
  m_colourbitdepth = 1 ; // 1 bit
  m_fg_r = m_fg_g = m_fg_b = m_fg_a = 0;
  m_bg_r = m_bg_g = m_bg_b = m_bg_a = 255;
  m_bg_grey = 255;
  m_fg_grey = 0 ;
}
DisplayImage::~DisplayImage()
{
  if (!m_bResourceImage && m_img) delete[] m_img ; // remove allocated image
}

DisplayImage& DisplayImage::operator=(const DisplayImage &img)
{
  m_img = NULL ;
  m_width = img.m_width ;
  m_height = img.m_height ;
  m_bResourceImage = img.m_bResourceImage ;
  m_memsize = img.m_memsize ;
  m_stride = img.m_stride;
  m_colourbitdepth = img.m_colourbitdepth ; 
  m_fg_r = img.m_fg_r;
  m_fg_g = img.m_fg_g;
  m_fg_b = img.m_fg_b;
  m_fg_a = img.m_fg_a;
  m_bg_r = img.m_bg_r;
  m_bg_g = img.m_bg_g;
  m_bg_b = img.m_bg_b;
  m_bg_a = img.m_bg_a;
  m_bg_grey = img.m_bg_grey;
  m_fg_grey = img.m_fg_grey;

  if (m_bResourceImage){
    m_img = img.m_img ;
  }else{
    if (createImage(m_width, m_height, m_colourbitdepth))
      copy(img) ;
    else
      throw "Cannot create new image" ;
  }

  return *this ;
}


// Custom error handler for the jpeg library.
static void jpgfile_error_exit(j_common_ptr cinfo)
{
  // Display error message
  (*cinfo->err->output_message) (cinfo);
  
  throw -1 ;
}

#define to565(r,g,b)                                            \
  ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

#define from565_r(x) ((((x) >> 11) & 0x1f) * 255 / 31)
#define from565_g(x) ((((x) >> 5) & 0x3f) * 255 / 63)
#define from565_b(x) (((x) & 0x1f) * 255 / 31)

uint16_t* DisplayImage::out565(uint16_t *outbuff, bool bRle)
{
  uint16_t *pOut = NULL, *p = NULL, last = 0, count = 0, colour = 0;
  if (!m_img) return NULL ; // no image

  if (m_colourbitdepth == 32 || m_colourbitdepth == 24 || m_colourbitdepth == 8){
    // convert to 16 bit colour depth
    if (outbuff){
      pOut = outbuff ;
    }else{
      if (bRle){
	// Worst case, RLE can be 2x as big as the original file if every pixel
	// is different
	pOut = new uint16_t[m_width * m_height *2] ;
	memset(pOut, 0, m_width * m_height * 2) ;
      }else{
	pOut = new uint16_t[m_width * m_height] ;
	memset(pOut, 0, m_width * m_height) ;
      }
    }
    p = pOut ;
    if (!pOut) return NULL ;

    for (unsigned int i=0; i < m_width * m_height; i++){
      if (m_colourbitdepth == 32 || m_colourbitdepth == 24){
	colour = to565(m_img[(i*(m_colourbitdepth/8))], m_img[(i*(m_colourbitdepth/8))+1], m_img[(i*(m_colourbitdepth/8))+2]);
      }else if (m_colourbitdepth == 8){
	colour = to565(m_img[i], m_img[i], m_img[i]) ;
      }
      if (bRle){
	if (count > 0 && colour == last && count < 65535){
	  count++;
	  continue;
	}else if (count > 0){
	  *p++ = count ;
	  *p++ = last ;
	}
	last = colour ;
	count = 1 ;
      }else{
	*p++ = colour ;
      }	    
    }
    if (bRle && count > 0){
      *p++ = count ;
      *p++ = last;
    }
  }else{
    return NULL ; // not yet supported
  }
  return pOut ;
}

uint8_t DisplayImage::to4bit(uint8_t byte)
{
  uint8_t out ;

  //out = (uint8_t)round(byte/16.0) ;
  //if (out > 15) out = 15;

  out = byte >> 4 ;
  
  return out ;
}

bool DisplayImage::createDistribution()
{
  unsigned int i =0 ;

  if (!m_img) return false ; // no image
  
  // Clear distribution information
  for (i=0;i<256;i++)m_red_distribution[i] = 0 ;
  for (i=0;i<256;i++)m_green_distribution[i] = 0 ;
  for (i=0;i<256;i++)m_blue_distribution[i] = 0 ;

  if (m_colourbitdepth == 32){
    for (i=0; i < m_width * m_height; i++){
      m_red_distribution[m_img[(i*4)]]++ ;
      m_green_distribution[m_img[(i*4)+1]]++ ;
      m_blue_distribution[m_img[(i*4)+2]]++ ;
    }
  }else{
    // Unsupported colour depth for distribution
    return false ;
  }
  return true ;
}

bool DisplayImage::setColourPixel(unsigned int x, unsigned int y, unsigned char r, unsigned char g, unsigned char b)
{
  if (m_colourbitdepth != 32){
    // unsupported at this time
    return false ;
  }
  unsigned int pixel = (x + (y*m_width)) * 4 ;

  m_img[pixel] = r ;
  m_img[pixel+1] = g;
  m_img[pixel+2] = b ;

  return true ;
}

unsigned int DisplayImage::getRedDistribution(uint8_t intensity)
{
  return m_red_distribution[intensity] ;
}

unsigned int DisplayImage::getGreenDistribution(uint8_t intensity)
{
  return m_green_distribution[intensity] ;
}

unsigned int DisplayImage::getBlueDistribution(uint8_t intensity)
{
  return m_blue_distribution[intensity] ;
}

bool DisplayImage::loadJPG(char *szFilename, unsigned int bits)
{
  struct jpeg_decompress_struct cinfo ;
  struct jpeg_error_mgr jerr ;
  FILE *f = NULL ;
  JSAMPARRAY pJpegBuffer ;
  int dataread = 0 ;
  unsigned int basepixel = 0 ; // image index
  
  if (!(f = fopen(szFilename, "rb"))){
    fprintf(stderr, "Cannot open JPEG image %s\n", szFilename) ;
    return false ;
  }
  
  cinfo.err = jpeg_std_error(&jerr) ;
  jerr.error_exit = jpgfile_error_exit;

  try{
    jpeg_create_decompress(&cinfo) ;
    jpeg_stdio_src(&cinfo, f) ;
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK){
      fprintf(stderr, "Failed to read JPEG file %s\n", szFilename) ;
      jpeg_destroy_decompress(&cinfo) ;
      return false ;
    }

    if (bits == 32 || bits == 24 || bits == 16){
      // This is the supported colour space we need
      cinfo.out_color_space = JCS_RGB ;
    }else if (bits == 8){
      cinfo.out_color_space = JCS_GRAYSCALE ;
    }else{
      fprintf(stderr, "Unsupported colour bit depth\n") ;
      jpeg_destroy_decompress(&cinfo) ;
      return false ;
    }

    jpeg_start_decompress(&cinfo) ;

    pJpegBuffer = (*cinfo.mem->alloc_sarray) (
					      (j_common_ptr)&cinfo,
					      JPOOL_IMAGE,
					      cinfo.output_width * cinfo.output_components,
					      1) ;

    if (cinfo.output_components != 1 && bits == 8){
      fprintf(stderr, "Mismatch of expected compoents. JPEG lib provides %d for greyscale\n", cinfo.output_components) ;
      return false ;
    }

    // Allocate for image
    if (!allocateImg(cinfo.output_width, cinfo.output_height, bits)){
      fprintf(stderr, "Error allocating image memory\n") ;
      return false ;
    }

    unsigned short n16bit = 0;
    //printf ("Allocateed image %d x %d\n", cinfo.output_width, cinfo.output_height) ;
    while (cinfo.output_scanline < cinfo.output_height){
      dataread = jpeg_read_scanlines(&cinfo, pJpegBuffer, 1) ;
      if (dataread <= 0) continue ; // should implement a check to ensure if this is blocked we can break out.

      //printf("Processing scanline %d, data read %d, image width %d\n", cinfo.output_scanline,dataread,cinfo.output_width) ;
      for (unsigned int i=0; i < m_width; i++){
	basepixel = (i * bits/8) + ((cinfo.output_scanline-1) * m_stride) ;

	if (bits == 32){
	  m_img[basepixel] = pJpegBuffer[0][i*3] ;
	  m_img[basepixel+1] = pJpegBuffer[0][(i*3)+1] ;
	  m_img[basepixel+2] = pJpegBuffer[0][(i*3)+2] ;
	  m_img[basepixel+3] = 0 ;
	}else if(bits == 24){
	  m_img[basepixel] = pJpegBuffer[0][i*3] ;
	  m_img[basepixel+1] = pJpegBuffer[0][(i*3)+1] ;
	  m_img[basepixel+2] = pJpegBuffer[0][(i*3)+2] ;	  
	}else if(bits == 16){
	  n16bit = to565(pJpegBuffer[0][i*3], pJpegBuffer[0][(i*3)+1], pJpegBuffer[0][(i*3)+2]);
	  m_img[basepixel] = n16bit >> 8 ;
	  m_img[basepixel+1] = 0x00FF & n16bit ;
	}else if(bits == 8){
	  m_img[basepixel] = pJpegBuffer[0][i] ;
	}
      }
    }
  }catch(...){
    fclose (f) ;
    jpeg_destroy_decompress(&cinfo) ;
    return false ;
  }
  fclose (f) ;
  jpeg_finish_decompress(&cinfo) ;
  jpeg_destroy_decompress(&cinfo) ;

  return true ;
}

bool DisplayImage::loadXBM(unsigned int w, unsigned int h, unsigned char *bits)
{
  if (bits == NULL || h == 0 || w == 0) return false ;
  m_height = h ;
  m_width = w ;

  m_bResourceImage = true ;
  m_img = bits ; // Just copy the pointer. The image is constant

  m_memsize = 0 ; // no memory allocated

  m_stride = w/8 + (w%8?1:0);
  
  return true ;
}

bool DisplayImage::drawLine(int x0, int y0, int x1, int y1)
{

  // work out simple drawing cases
  if (x0 == x1) return drawV(x0, y0, y1) ;
  if (y0 == y1) return drawH(x0, x1, y0) ;

  int dx, dy, incrH, incrHV, d, x, y ;

  // Check if this is x incrementing line or y based on direction of gradient
  bool bIncX = abs(x0-x1) >= abs(y0-y1) ;
  int *adj0, *adj1, *opp0, *opp1 ;

  if (bIncX){
    adj0 = &x0 ;
    adj1 = &x1 ;
    opp0 = &y0 ;
    opp1 = &y1 ;
  }else{
    adj0 = &y0 ;
    adj1 = &y1 ;
    opp0 = &x0 ;
    opp1 = &x1 ;
  }
  
  // From this point on when x and y are discussed they are part of 
  // a transformed virtual x and y for the purpose of the mid-point
  // algorithm
  dx = *adj1-*adj0 ; 
  dy = *opp1-*opp0 ; 
  d = abs(dy) * 2 - abs(dx) ; 
  incrH = abs(dy) * 2; // Horiz increment
  incrHV = (abs(dy)-abs(dx)) * 2 ; // Vert & Horiz increment
  x = *adj0 ;
  y = *opp0 ;
  
  if (y >=0 && x >= 0){ 
    if (bIncX)setPixel(x,y,true) ;
    else setPixel(y,x,true) ;
  }

  while (x != *adj1){
    if (d <= 0){
      d += incrH ;
      x += dx>0?1:-1;
    }else{
      d += incrHV ;
      x += dx>0?1:-1;
      y += dy>0?1:-1;
    }
    if (y >=0 && x >= 0){
      if (bIncX)setPixel(x,y,true) ;
      else setPixel(y,x,true) ;
    }
  }

  return true ;
}

bool DisplayImage::drawRect(int x0, int y0, int width, int height, bool bFill)
{
  bool bRet = true ;
  
  if (!drawV(x0,y0,y0+height))bRet = false ;
  if (!drawV(x0+width,y0,y0+height))bRet =  false ;

  if (bFill){
    for (int i=0; i<height;i++){
      if (!drawH(x0,x0+width,y0+i)) bRet = false; 
    }
  }else{
    if (!drawH(x0,x0+width,y0))bRet = false ;
    if (!drawH(x0,x0+width,y0+height))bRet = false ;
  }    
  
  return bRet ;
}

bool DisplayImage::drawV(int x, int y0, int y1)
{
  int inc = 1, cy = 0 ;
  if (y0 > y1) inc = -1 ; // need to reverse writing direction

  // Handle exception cases
  if (x < 0 || x >= (int)m_width) return false ; // Out of image area for entire line
  if (y0 < 0 && y1 < 0) return false ; // out of image area for entire line
  if (y0 >= (int)m_height && y1 >= (int)m_height) return false ; // out of image area for entire line

  for (cy=y0; cy != y1; cy+=inc){
    if (cy >= 0 && cy < (int)m_height) setPixel(x, cy, true) ;
  }
  if (cy >=0) setPixel(x, cy, true); // set final x,y1 pixel

  return true ;
}

bool DisplayImage::drawH(int x0, int x1, int y)
{
  int inc = 1, cx = 0 ;
  if (x0 > x1) inc = -1 ; // need to reverse writing direction

  // Handle exception cases
  if (y < 0 || y >= (int)m_height) return false ; // Out of image area for entire line
  if (x0 < 0 && x1 < 0) return false ; // out of image area for entire line
  if (x0 >= (int)m_width && x1 >= (int)m_width) return false ; // out of image area for entire line

  for (cx=x0; cx != x1; cx+=inc){
    if (cx >= 0 && cx < (int)m_width) setPixel(cx, y, true) ;
  }
  if (cx >=0) setPixel(cx, y, true); // set final x1,y pixel

  return true ;  
}

bool DisplayImage::loadFile(int f)
{
  if (!f) return false ; // need an open file

  uint32_t width = 0 ;
  uint32_t height = 0 ;

  // Not great as it assumes little endian packing (or depends on hardware big/little endian configuration)
  
  // Read the width and height
  if (read(f, &width, sizeof(uint32_t)) != sizeof(uint32_t)){
    return false ;
  }
  if (read(f, &height, sizeof(uint32_t)) != sizeof(uint32_t)){
    return false ;
  }
  if (!allocateImg(width,height,1)) return false ;
  
  // casting to signed shouldn't be a problem as we have a configured file limit under 2 GB
  // and large image files are just wrong for OLEDs
  if (read(f, m_img, m_memsize) != (signed)m_memsize){
    return false ; // couldn't read all of the image
  }

  return true ;
}
 
bool DisplayImage::createImage(unsigned int width, unsigned int height, unsigned int bitdepth)
{
  if (!allocateImg(width, height, bitdepth)) return false ;

  return zeroImg() ;
}

bool DisplayImage::allocateImg(unsigned int width, unsigned int height, unsigned int bitdepth)
{
  unsigned int size = 0, stride =0;

  if (width == 0 || height == 0){
    // This is an error but can be treated as
    // an empty image
    if (m_img) delete[] m_img ;
    m_img = NULL;
    m_memsize = 0 ;
    m_width = 0 ;
    m_height = 0 ;
    m_stride = 0 ;
    return true ;
  }
    
  // packed byte file should hold ceil(x/8) * y bytes rounded up to whole byte per row.
  if (bitdepth == 1){
    stride = width/8 + (width%8?1:0);
    size = stride * height ;
    if (size > XMB_LOAD_MAX_SIZE){
      // File is very large, likely to be corrupt or not an image file
      return false ;
    }
  }else if(bitdepth == 32){
    // RGBA
    stride = 4 * width ;
    size = stride * height ;
  }else if(bitdepth == 16){
    // RGB 565
    stride = 2 * width ;
    size = stride * height ;
  }else if(bitdepth == 8){
    // Greyscale
    stride = width ;
    size = stride * height ;
  }else{
    // Unsupported
    return false ;
  }

  m_colourbitdepth = bitdepth ;

  if (!m_bResourceImage && m_img) delete[] m_img ; // remove old image and replace with this one.
  m_bResourceImage = false ; // this is an image which can be removed from memory
  
  // Allocate memory
  m_img = new unsigned char[size] ;
  if (!m_img) return false ; // cannot allocate memory for image

  m_memsize = size ;
  m_width = width ;
  m_height = height ;
  m_stride = stride ;
 
  return true ;
}

bool DisplayImage::zeroImg()
{
  if (m_bResourceImage) return false ;
  memset(m_img, 0, m_memsize) ;
  return true ;
}

bool DisplayImage::eraseBackground()
{
  unsigned int pixel = 0 ;
  unsigned short n16bit = 0 ;

  if (m_colourbitdepth == 1){
    return zeroImg() ;
  }

  for (unsigned int cy=0; cy < m_height; cy++){
    for (unsigned int cx=0; cx < m_width; cx++){
      if (m_colourbitdepth == 32){
	pixel = (cx*4)+(cy*m_stride) ;
	m_img[pixel] = m_bg_r ;
	m_img[pixel+1] = m_bg_g ;
	m_img[pixel+2] = m_bg_b ;
	m_img[pixel+3] = m_bg_a ;
      }else if(m_colourbitdepth == 16){
	pixel = (cx*2)+(cy*m_stride) ;
	n16bit = to565(m_bg_r, m_bg_g, m_bg_b) ;
	m_img[pixel] = n16bit >> 8;
	m_img[pixel+1] = n16bit & 0x00FF ;
      }else if(m_colourbitdepth == 8){
	pixel = cx + (cy*m_stride) ;
	m_img[pixel] = m_bg_grey ;
      }else{
	return false ; // Not supported
      }
    }
  }
  return true ;
}

bool DisplayImage::copy(const DisplayImage &img, int mode, unsigned int offx, unsigned int offy)
{
  unsigned int despixel = 0;
  unsigned int srcpixel = 0 ;

  // Colour bit depths should match. I could implement 8 to 32 and 32 to 8 conversion
  // but this code grows quickly to include 16bit colour and other colour depths. 
  // The rule needs to be that the bit depths match and any future need to mismatch images will require
  // an image conversion routine to change bit depth.
  if (img.m_colourbitdepth != m_colourbitdepth) return false ;
  if (m_colourbitdepth != 32 && 
      m_colourbitdepth != 16 && 
      m_colourbitdepth != 8){
    return false ; // only 32/16/8 bit images supported at the moment
  }

  for (unsigned int cy=0; cy < m_height; cy++){
    for (unsigned int cx=0; cx < m_width; cx++){
      if ((cx - offx) >= 0 && (cx - offx) < img.m_width && (cy - offy) >= 0 && (cy - offy) < img.m_height){
	// Within the drawable area for the parent image
	despixel = (cx*(m_colourbitdepth/8))+(cy*m_stride) ;
	srcpixel = ((cx-offx)*(m_colourbitdepth/8)) + ((cy-offy)*img.m_stride);

	if (mode == 1){ // XOR
	  for (unsigned int cbd=0; cbd < m_colourbitdepth/8;cbd++){
	    m_img[despixel+cbd] ^= img.m_img[srcpixel+cbd] ;
	  }
	}else if(mode == 2){ // Invert OR
	  for (unsigned int cbd=0; cbd < m_colourbitdepth/8;cbd++){
	    m_img[despixel+cbd] = ~(~(m_img[despixel+cbd]) | ~(img.m_img[srcpixel+cbd])) ;
	  }
	}else if(mode == 4){ // 'Max Colour' Transparency
	  for (unsigned int cbd=0; cbd < m_colourbitdepth/8;cbd++){
	    m_img[despixel+cbd] = (img.m_img[srcpixel+cbd] == 255)?m_img[despixel+cbd]:img.m_img[srcpixel+cbd] ;
	  }
	}else{ // Overwrite
	  for (unsigned int cbd=0; cbd < m_colourbitdepth/8;cbd++){
	    m_img[despixel+cbd] = img.m_img[srcpixel+cbd] ;
	  }
	}
      }
    }
  }
  return true ;
}

inline bool DisplayImage::setPixel(unsigned int x, unsigned int y, bool bSet)
{
  unsigned short n16bit = 0;

  if (m_colourbitdepth == 16){
    if (bSet)
      n16bit = to565(m_fg_r, m_fg_g, m_fg_b) ;
    else
      n16bit = to565(m_bg_r, m_bg_g, m_bg_b) ;
  }

  if (x >= m_width || y >= m_height) return false ; // out of image boundary
  
  if (bSet){
    if (m_colourbitdepth == 32){
      m_img[(x*4) + y*m_stride] = m_fg_r ;
      m_img[(x*4) + y*m_stride+1] = m_fg_g ;
      m_img[(x*4) + y*m_stride+2] = m_fg_b ;
      m_img[(x*4) + y*m_stride+3] = m_fg_a ;
    }else if (m_colourbitdepth == 16){
      m_img[(x*2) + y*m_stride] = n16bit >> 8 ;
      m_img[(x*2) + y*m_stride+1] = 0x00FF & n16bit ;
    }else if (m_colourbitdepth == 8){
      m_img[x + y*m_stride] = m_fg_grey ;
    }else if (m_colourbitdepth == 1){
      m_img[(x/8)+y*m_stride] |= 1 << (x%8) ;
    }
  }else{
    if (m_colourbitdepth == 32){
      m_img[(x*4) + y*m_stride] = m_bg_r ;
      m_img[(x*4) + y*m_stride+1] = m_bg_g ;
      m_img[(x*4) + y*m_stride+2] = m_bg_b ;
      m_img[(x*4) + y*m_stride+3] = m_bg_a ;
    }else if (m_colourbitdepth == 16){
      m_img[(x*2) + y*m_stride] = n16bit >> 8 ;
      m_img[(x*2) + y*m_stride+1] = 0x00FF & n16bit ;
    }else if (m_colourbitdepth == 8){
      m_img[x + y*m_stride] = m_bg_grey ;
    }else if (m_colourbitdepth == 1){
      m_img[(x/8)+y*m_stride] &= ~(1 << (x%8)) ;
    }
  }
  return true ;
}

void DisplayImage::printImg()
{
  printf("Printing image height %d and width %d\n", m_height, m_width) ;
  uint32_t stride = m_width/8 + (m_width%8 >0?1:0) ;
  for (uint32_t cy=0; cy < m_height; cy++){
    for (uint32_t cx=0; cx < m_width; cx++){
      if ((m_img[(cx/8)+cy*stride] & (1 << (cx%8))) > 0) printf ("#") ;
      else printf(" ") ;
    }
    printf ("\n") ;
  }
}


DisplayFont::DisplayFont()
{
  m_pBuffer = NULL ;
  m_nFontWidth = 0 ;
  m_nFontHeight = 0;
  m_nTotalChars = 0;
}

DisplayFont::~DisplayFont()
{
  if (m_pBuffer) delete[] m_pBuffer ;
}

bool DisplayFont::loadFile(int f)
{
  if (!f) return false ; // need an open file

  uint32_t size = 0 ;
  uint32_t width = 0 ;
  uint32_t height = 0 ;
  uint32_t chars = 0;
  unsigned char *buffer = NULL ;
  char szSig[4] ;

  // Not great as it assumes little endian packing (or depends on hardware big/little endian configuration)

  // Read and check sig
  if (read(f, szSig, 3) != 3) return false ;
  szSig[3] = '\0' ; // terminate string
  if (strcmp(szSig, "FNT") != 0){
    fprintf(stderr, "Cannot open the font file, invalid signature\n") ;
    return false ; // not a font file
  }
  
  // Read the number of supported characters in file
  if (read(f, &chars, sizeof(uint32_t)) != sizeof(uint32_t)){
    return false ;
  }

  // Read the width and then height of each font
  if (read(f, &width, sizeof(uint32_t)) != sizeof(uint32_t)){
    return false ;
  }
  if (read(f, &height, sizeof(uint32_t)) != sizeof(uint32_t)){
    return false ;
  }

  // packed byte file should hold ceil(x/8) * y bytes rounded up to whole byte per row.
  size = (width/8 + (width%8?1:0)) * height ;
  size *= chars ; // multiply by the number of characters in the file
  if (size > XMB_LOAD_MAX_SIZE){
    // File is very large, likely to be corrupt or not an image file
    return false ;
  }

  // Allocate memory
  buffer = new unsigned char[size] ;
  if (!buffer) return false ; // cannot allocate memory for image

  // casting to signed shouldn't be a problem as we have a configured file limit under 2 GB
  // and large image files are just wrong for OLEDs
  if (read(f, buffer, size) != (signed)size){
    return false ; // couldn't read all of the image
  }

  if (!m_pBuffer) delete[] m_pBuffer ; // remove old image and replace with this one.

  // Write all attributes of the font to object
  m_pBuffer = buffer ;
  m_nFontWidth = width ;
  m_nFontHeight = height ;
  m_nTotalChars = chars ;

  return true ;
}
DisplayImage *DisplayFont::createText(char *szTxt, DisplayImage *cimg)
{
  unsigned char letter = '*' ;
  uint32_t writetocol = 0 ; // update to point at start of new letter
  uint32_t readbyte = 0, writebyte = 0 ;
  uint32_t fontstride = 0 ;
  int nLen = strlen(szTxt) ;
  int nLines = 1, onLine = 0, onCharCol = 0 ;
  if (nLen == 0) return NULL ; // No string to show

  DisplayImage *img = NULL ;
  
  // Reuse an image if cimg is set
  if (cimg) img = cimg ;
  else{
    // Make a new image to write to
    img = new DisplayImage ;
    if (!img) return NULL ; // Memory error
  }
  
  if (!cimg){
    // Check text for new lines
    char *pstr = szTxt ;
    while (*pstr != '\0') if (*pstr++ == '\n') nLines++ ; 
    // Allocate space in the image buffer unless reusing a buffer
    if (!img->allocateImg(nLen * m_nFontWidth, m_nFontHeight*nLines,1)) return NULL ; // Memory error
  }

  img->zeroImg() ; // Clear the memory
  
  // Calculate width of each row in bytes
  fontstride = m_nFontWidth/8 +(m_nFontWidth%8?1:0) ;

  // Iterate through the letters
  for (int i=0; i<nLen; i++){
    letter = szTxt[i] ;
    
    if (letter == '\n'){
      onLine++ ; // Increment the line we are processing
      onCharCol = 0 ; // Do a carriage return
      continue ; // don't do anymore processing as this is non-printable
    }
    
    if (letter >= m_nTotalChars) letter = 0 ; // Cannot exceed number of letters in font image

    // determine starting column in output text. Increment to next char after call 
    writetocol = m_nFontWidth * onCharCol++ ;
    
    for (uint32_t cy=0; cy < m_nFontHeight; cy++){
      for (uint32_t cx=0; cx < m_nFontWidth;cx++){
	writebyte = (cx+writetocol)/8 + (cy * img->m_stride) + (img->m_stride * onLine * m_nFontHeight) ;

	if (writebyte >= img->m_memsize) return img ; // no more memory to write to. Stop processing
	
	readbyte = cx/8 + (cy * fontstride) + (fontstride * letter * m_nFontHeight) ;
	if ((m_pBuffer[readbyte] & (1 << (cx%8))) > 0){
	  img->m_img[writebyte] |= 1 << ((cx+writetocol)%8) ; // set image bit to on
	}
	else{
	  img->m_img[writebyte] &= ~(1 << ((cx+writetocol)%8)) ; // set image bit to off
	}
      }
    }
  }
  
  return img ;
}
