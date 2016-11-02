#ifndef __DISPLAYIMAGE_HW_HPP
#define __DISPLAYIMAGE_HW_HPP

#include <stdint.h>
#include <stddef.h>

#ifdef DISPLAY_SDD1306OLED
class SDD1306OLED ; // OLED display using SDD1306
#endif
#ifdef DISPLAY_PCF8833LCD
class PCF8833LCD ; // Philips Colour LCD Display
#endif

class DisplayFont ; 

// Generous 10MB image limit for single image files
#define XMB_LOAD_MAX_SIZE 10485760

class DisplayImage{
public:
  DisplayImage() ;
  ~DisplayImage();
#ifdef DISPLAY_SDD1306OLED
  friend class SDD1306OLED ;
#endif
#ifdef DISPLAY_PCF8833LCD
  friend class PCF8833LCD ;
#endif
  friend class DisplayFont ;

  // Create blank image. Basically a call to allocateImg but with a follow up to
  // make the image blank. Will destroy old image if already exists.
  bool createImage(unsigned int width, unsigned int height, unsigned int bitdepth) ;

  // Draw a rectangle. Set bFill to true to fill the rectangle
  // false is returned if any of the lines in the rectangle could not be drawn due to being outside the
  // viewable area. Return values can be mostly ignored.
  bool drawRect(int x0, int y0, int width, int height, bool bFill=false);
  
  // Draw straight lines across the image. Start and end points do not need to 
  // be within the bounds of the image and will be clipped.
  // Returns false if the line cannot be drawn at all, but this is more for information than an error.
  bool drawLine(int x0, int y0, int x1, int y1) ;

  // Load from existing header file. Used when XBM resource is built into the
  // executable binary
  bool loadXBM(unsigned int w, unsigned int h, unsigned char *bits) ;

  // Load a 24 bit jpeg into the image object (becomes 32bit with alpha for 32)
  // Supports greyscale when bits is 8.
  bool loadJPG(char *szFilename, unsigned int bits = 32) ;
  
  // Load a custom binary representation from file.
  // Use XBM2Bin utility to create
  bool loadFile(int f) ;

  // Set all bits to zero, clearing the image values
  bool zeroImg() ;

  // Erase the background using the background colour
  // Works with 32, 16, 8 and 1 bit colour depths
  bool eraseBackground() ;
  
  // create a character representation of the image for terminal
  // useful for debug and not much else
  void printImg() ;

  // Set the value of a pixel. Use bSet to set as ON of OFF with true/false values
  // Works for 32, 16, 8 and 1 bit colour depths
  bool setPixel(unsigned int x, unsigned int y, bool bSet) ;

  // Set a colour pixel. setPixel also draws colour by using setBGCol and setFGCol as an alternative
  // This is only for 32 bit image formats.
  bool setColourPixel(unsigned int x, unsigned int y, unsigned char r, unsigned char g, unsigned char b) ;

  // Create distribution. Returns false if no image or colour depth unsupported
  // This is for colour RGB images. Returns false for 1bit images.
  bool createDistribution() ;

  // Get number of red pixels at the value set by intensity contained in the image
  // intensity is 0-255
  unsigned int getRedDistribution(uint8_t intensity) ;

  // Get number of green pixels at the value set by intensity contained in the image
  // intensity is 0-255
  unsigned int getGreenDistribution(uint8_t intensity) ;

  // Get number of blue pixels at the value set by intensity contained in the image
  // intensity is 0-255
  unsigned int getBlueDistribution(uint8_t intensity) ;

  // Utility function. Coverts 8bit colour to 4bit.
  static uint8_t to4bit(uint8_t byte) ;

  // Create 16 bit colour image. Not used internally so image
  // will retain 32 bits. Buffer must be delete[] after use.
  uint16_t* out565(uint16_t *outbuff=NULL, bool bRle=false);

  // Copy the image to this objects image. Can be offset by offx and offy
  bool copy(DisplayImage &img, int mode=0, unsigned int offx=0, unsigned int offy=0) ;

  void setBGGrey(unsigned char grey){m_bg_grey = grey;};

  void setFGGrey(unsigned char grey){m_fg_grey = grey;};

  void setBGCol(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha){m_bg_r = red;m_bg_g = green; m_bg_b=blue;m_bg_a = alpha;};

  void setFGCol(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha){m_fg_r = red;m_fg_g = green; m_fg_b=blue;m_fg_a = alpha;};

protected:
  // Draw vertical lines. Used internally, but not needed for users as
  // this is called by draw methods when required
  bool drawV(int x, int y0, int y1);

  // Draw horizonal lines from x0 to x1 along y.
  // Not required for direct user access but used by other draw methods
  bool drawH(int x0, int x1, int y);

  bool allocateImg(unsigned int height, unsigned int width, unsigned int bitdepth) ;
  unsigned char *m_img ;
  unsigned int m_memsize ;
  unsigned int m_width ;
  unsigned int m_height ;
  unsigned int m_stride ;
  bool m_bResourceImage ;
  unsigned int m_colourbitdepth ;
  unsigned int m_red_distribution[256] ;
  unsigned int m_green_distribution[256] ;
  unsigned int m_blue_distribution[256] ;
  
  unsigned char m_fg_r, m_fg_g, m_fg_b, m_fg_a;
  unsigned char m_bg_r, m_bg_g, m_bg_b, m_bg_a;
  unsigned char m_fg_grey, m_bg_grey ;
};

class DisplayFont{
public:
  DisplayFont();
  ~DisplayFont();

  // Load font from file. Use utility
  // to convert PSF compressed files to a binary format to load
  bool loadFile(int f) ;

  // Create a buffer with a text string to display
  // This returns an DisplayImage which can be written to the display
  // The returned image will need to be deleted by the caller
  DisplayImage *createText(char *szTxt, DisplayImage *cimg = NULL) ;

protected:
  uint32_t m_nFontWidth ;
  uint32_t m_nFontHeight ;
  uint32_t m_nTotalChars ;
  unsigned char *m_pBuffer ;
};


#endif
