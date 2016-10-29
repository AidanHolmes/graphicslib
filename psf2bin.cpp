#include <ft2build.h>
#include <ftbitmap.h>
#include <stdio.h>
#include <stdint.h>

#include FT_FREETYPE_H
//#include FT_OUTLINE_H
#include FT_BITMAP_H

#define INFOSWITCH "-info"

int initialiseparam(int argc, char **argv, bool *bSetInfo, char **pSource, FILE **fout)
{
  FILE *fin = NULL ;
  
  if (argc <= 1 || argc > 3){
    printf("Usage: psf2bin font.psf [-info] [output.bin]\n\tOptional output file, otherwise outputs to stdout\n");
    printf("\t-info can be used with a font file to query the font to be converted\n") ;
    return -1 ;
  }

  *pSource = argv[1] ; // Assume the first param is the source font file
  
  // Check for the information flag
  if (argc > 2){
    if (strcmp(argv[1], INFOSWITCH) == 0 || strcmp(argv[2], INFOSWITCH) == 0){
      if (argv[1][0] == '-') *pSource = argv[2] ; // switch is the first parameter, swap source file
      *bSetInfo = true ;
    }
  }
  
  if (!(fin=fopen(*pSource, "r"))){
    fprintf(stderr, "Cannot open %s\n", *pSource) ;
    return -1 ;
  }
  fclose(fin) ; // FreeType handles the IO to process the file so close this.

  // Open file to write result to
  if (argc > 2 && !*bSetInfo){
    if (!(*fout=fopen(argv[2], "w"))){
      fprintf(stderr, "Cannot write to output file %s\n", argv[2]) ;
      return -1 ;
    }
  }else{
    *fout = stdout ;
  }

  return 0 ;
}
char *tobinary(int val)
{
  static char b[33] ;
  for (int i=0; i < 32; i++){
    b[i] = ((val << i) & 0x01)?'1':'0' ;
  }
  b[32] = '\0' ;
  return b ;
}

int printFaceInfo(FT_Library library, char *szSource)
{
  FT_Face face ;

  if (FT_New_Face(library, szSource, 0,&face) != 0){
    fprintf(stderr, "Cannot open %s to access the font face details\n", szSource) ;
    return -1 ;
  }
  printf ("Information relating to first face in library\n\n") ;

  printf ("Library contains %ld faces\n", face->num_faces) ;
  printf ("This is index %ld\n", face->face_index) ;
  printf ("Family name: %s\nStyle name: %s\n", face->family_name, face->style_name) ;
  printf ("Flags:\n") ;
  printf ("\tFace - %s\n", tobinary(face->face_flags)) ;
  printf ("\tStyle - %s\n", tobinary(face->style_flags)) ;
  printf ("Glyphs: %ld\n", face->num_glyphs) ;
  printf ("%d fixed sizes available:\n", face->num_fixed_sizes) ;
  for (int isizes=0; isizes < face->num_fixed_sizes; isizes++){
    printf("\tHeight: %d, Width: %d, Size: %ld, XPPEM: %ld, YPPEM: %ld\n",
	   face->available_sizes[isizes].height,
	   face->available_sizes[isizes].width,
	   face->available_sizes[isizes].size,
	   face->available_sizes[isizes].x_ppem,
	   face->available_sizes[isizes].y_ppem);
  }
  printf ("%d charmaps available:\n", face->num_charmaps) ;
  for (int imap=0; imap < face->num_charmaps; imap++){
    printf ("\tPlatform ID: %d, ", face->charmaps[imap]->platform_id) ;
    printf ("Encoding ID: %d ", face->charmaps[imap]->encoding_id) ;
    if (face->charmaps[imap] == face->charmap) printf("[Active]") ; // Handles appear to be comparable
    printf("\n") ;
  }

  return 0 ;
}
unsigned int writeBitmap(FT_Bitmap bm, unsigned char *pbuff)
{
  if (bm.pixel_mode != FT_PIXEL_MODE_GRAY){
    // Can only process 8bit grey colour
    fprintf(stderr, "Cannot process pixel_mode %d. The programmer is lazy\n", bm.pixel_mode) ;
    return 0 ;
  }

  unsigned char *p = bm.buffer, *r = NULL ;
  // Bit packed stride width in bytes
  unsigned int stride = bm.width/8 +(bm.width%8?1:0) ;
  
  for (unsigned int row=0; row < bm.rows; row++){
    r = p ;
    for (unsigned int col=0; col <bm.width; col++){
      if (*r > 0) pbuff[(col/8) + (row*stride)] |= 1 << (col%8) ;
      else pbuff[(col/8) + (row*stride)] &= ~(1 << (col%8)) ;
      r++ ;
    }
    p += bm.pitch ;
  }
  return stride * bm.rows ;
}
 
bool outputFontBitmap(FILE *f, FT_Short width, FT_Short height, unsigned char *pFontImage)
{
  char sig[] = "FNT" ;
  uint32_t chars = 256 ;
  uint32_t nWidth = width ;
  uint32_t nHeight = height ;
  // Write header
  if (fwrite(sig, 1,3,f) <= 0) return false ;
  if (fwrite(&chars, sizeof(uint32_t),1,f) <= 0) return false ;
  if (fwrite(&nWidth, sizeof(uint32_t),1,f) <= 0) return false ;
  if (fwrite(&nHeight, sizeof(uint32_t),1,f) <= 0) return false ;

  uint32_t image_buffer_size = (width/8 + (width%8?1:0)) * height * 256 ;

  // Write body
  if (fwrite(pFontImage, sizeof(unsigned char),image_buffer_size,f) <= 0) return false ;

  return true ;
}
 
int main(int argc , char **argv)
{
  FT_Library library ;
  FT_Face face ;
  FILE *fout = stdout ;
  char *szSource = NULL ;
  bool bInfo = false ;
  int ret = 0 ;
  FT_UInt glyph_index  = 0, image_buffer_size =0;
  FT_Bitmap charbitmap;
  unsigned char *pFontImage = NULL ;
  FT_Short bitmap_width = 0, bitmap_height = 0 ;
  int byteswritten = 0 ;

  ret = initialiseparam(argc, argv, &bInfo, &szSource, &fout) ;
  if (ret < 0) return ret;

  // Initialise the FreeType library
  if (FT_Init_FreeType(&library) != 0){
    fprintf(stderr, "Cannot initalise FreeType library\n") ;
    return -1 ;
  }

  if (bInfo){
    ret = printFaceInfo(library, szSource) ;
    return ret ;
  }
  
  // Query the first face in the library (set zero as the face index)
  if (FT_New_Face(library, szSource, 0,&face) != 0){
    fprintf(stderr, "Cannot open %s to access the font face details\n", szSource) ;
    return -1 ;
  }

  if (face->available_sizes == NULL){
    fprintf(stderr, "No bitmap strikes in file\n") ;
    return -1 ;
  }

  // Query the first size in the available sizes.
  // THIS MAY NOT BE THE RENDERED SIZE
  bitmap_width = face->available_sizes[0].width ;
  bitmap_height = face->available_sizes[0].height ;
  image_buffer_size = (bitmap_width/8 + (bitmap_width%8?1:0)) * bitmap_height * 256 ;

  // Allocate memory for the font image used by DisplayFont class
  pFontImage = new unsigned char[image_buffer_size] ;
  if (!pFontImage){
    fprintf(stderr, "Cannot allocate memory for font image buffer\n") ;
    return -1 ;
  }

  unsigned char *p = pFontImage ;
 
  // Iterate through all byte characters
  for(int fontindex=0; fontindex < 256; fontindex++){
    glyph_index = FT_Get_Char_Index(face, fontindex) ;
    if ((ret = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT ))) {
      fprintf( stderr, "warning: failed FT_Load_Glyph 0x%x %d\n", glyph_index, ret);
      return -1;
    }

    if ((ret = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_MONO ))) {
      fprintf(stderr, "warning: failed FT_Render_Glyph 0x%x %d\n", glyph_index, ret);
      return -1;
    }

    // FT_Bitmap_Init supported in 2.7. FT_Bitmap_New used for older instances of freetype so is backwards
    // compatible.
    //FT_Bitmap_Init(&charbitmap) ;
    FT_Bitmap_New(&charbitmap) ;

    // Convert to mono in charbitmap. Align to 1 byte
    if (FT_Bitmap_Convert(library, &face->glyph->bitmap, &charbitmap, 1) !=0){
      fprintf(stderr, "Failed to convert bitmap\n") ;
      return -1 ;
    }

    if ((FT_Short)charbitmap.width != bitmap_width ||
	(FT_Short)charbitmap.rows != bitmap_height){
      // Assertion error as height and width are different to expected size
      fprintf(stderr, "Expected height %d, width %d. Font is actually rows %d, width %d\n", bitmap_height, bitmap_width, charbitmap.rows, charbitmap.width) ;
      return -1 ;
    }

    byteswritten = writeBitmap(charbitmap, p) ;
    p += byteswritten ;
    
    //printf("Char %d, Glyph Index %u, Width %d, Rows %d, Pitch %d, Pixel Mode ", fontindex, glyph_index, charbitmap.width, charbitmap.rows, charbitmap.pitch) ;
    //if (charbitmap.pixel_mode == FT_PIXEL_MODE_GRAY) printf ("Grey, Num Greys %d\n", charbitmap.num_grays) ;
    //else if (charbitmap.pixel_mode == FT_PIXEL_MODE_MONO) printf ("Mono\n") ;
    //else printf("Other\n") ;


    FT_Bitmap_Done(library, &charbitmap);
  }

  if (!outputFontBitmap(fout, bitmap_width, bitmap_height, pFontImage)){
    fprintf(stderr, "Failed to write image to output\n") ;
    return -1 ;
  }

  if (fout != stdout) fclose (fout) ;
  FT_Done_FreeType(library) ;
  return 0 ;
}
