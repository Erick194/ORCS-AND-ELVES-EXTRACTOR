#include <windows.h>
#include <conio.h>
#include <stdio.h>

#include "funciones.h"
#include "zip.h"
#include "unzip.h"
#include <png.h>

using namespace std;

HZIP hz; DWORD writ;

typedef byte*  cache;
static cache writeData;
static unsigned int current = 0;

//**************************************************************
//**************************************************************
//  Png_WriteData
//
//  Work with data writing through memory
//**************************************************************
//**************************************************************

static void Png_WriteData(png_structp png_ptr, cache data, size_t length) {
    writeData = (byte*)realloc(writeData, current + length);
    memcpy(writeData + current, data, length);
    current += length;
}

//**************************************************************
//**************************************************************
//  Png_Create
//
//  Create a PNG image through memory
//**************************************************************
//**************************************************************

cache Png_Create(cache data, int* size, int width, int height, int paloffset, int offsetx = 0, int offsety = 0, int tans = -1)
{
    int i, j;
    cache image;
    cache out;
    cache* row_pointers;
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp palette;
    
    int bit_depth = 4;
    // setup png pointer
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(png_ptr == NULL) {
        error("Png_Create: Failed getting png_ptr");
        return NULL;
    }

    // setup info pointer
    info_ptr = png_create_info_struct(png_ptr);
    if(info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        error("Png_Create: Failed getting info_ptr");
        return NULL;
    }

    // what does this do again?
    if(setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        error("Png_Create: Failed on setjmp");
        return NULL;
    }

    // setup custom data writing procedure
    png_set_write_fn(png_ptr, NULL, Png_WriteData, NULL);

    // setup image
    png_set_IHDR(
        png_ptr,
        info_ptr,
        width,
        height,
        bit_depth,
        PNG_COLOR_TYPE_PALETTE,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    // setup palette
    FILE *fpal = fopen ("Datos/palettes.bin","rb");
    if(!fpal)
    {
       error("No puede abrir palettes.bin");
    }

    fseek(fpal,2 + (paloffset * 2),SEEK_SET);
    palette = (png_colorp) malloc((16*3)*png_sizeof(png_color));

    for(int x = 0;x < 16;x++)
    {
       int Palbit = ReadWord(fpal);
       int B = (Palbit & 0x1F) << 3;
       int G = (Palbit >> 5 & 0x3F) << 2;
       int R = (Palbit >> 11 & 0x3F) << 3;

       palette[x].red = R;
       palette[x].green = G;
       palette[x].blue = B;
    }
    fclose(fpal);
    
    png_set_PLTE(png_ptr, info_ptr,palette,16);
    
    if(tans != -1)
    {
        png_byte trans[tans+1]; 
        for(int tr =0;tr < tans+1; tr++)
        {
          if(tr==tans){trans[tr]=0;}
          else {trans[tr]=255;}
        }
        png_set_tRNS(png_ptr, info_ptr,trans,tans+1,NULL);
    }

    // add png info to data
    png_write_info(png_ptr, info_ptr);
    
    if(offsetx !=0 || offsety !=0)
    {
       int offs[2];
    
       offs[0] = Swap32(offsetx);
       offs[1] = Swap32(offsety);
    
       png_write_chunk(png_ptr, (png_byte*)"grAb", (byte*)offs, 8);
    }

    // setup packing if needed
    png_set_packing(png_ptr);
    png_set_packswap(png_ptr);

    // copy data over
    byte inputdata;
    image = data;
    row_pointers = (cache*)malloc(sizeof(byte*) * height);
    for(i = 0; i < height; i++)
    {
        row_pointers[i] = (cache)malloc(width);
        for(j = 0; j < width; j++)
        {
            inputdata = *image;
            if(inputdata == 0x7f){inputdata = (byte)tans;}
            row_pointers[i][j] = inputdata;
            image++;
        }
    }

    // cleanup
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    free((void**)&palette);
    free((void**)row_pointers);
    palette = NULL;
    row_pointers = NULL;
    png_destroy_write_struct(&png_ptr, &info_ptr);

    // allocate output
    out = (cache)malloc(current);
    memcpy(out, writeData, current);
    *size = current;

    free(writeData);
    writeData = NULL;
    current = 0;

    return out;
}

//--------Rotar 90º Y Flip--------//
static void rotate90andflip(byte *input, byte *output, int width, int height)
{
    int i;
    int length = (width*height);
    int height0 = height;//width
    int width0 = length / height;//height
    //printf("sizeof(input) %d\n",length);
    
    int bit1 = width0;
    int bit2 = 1;
    int bit3 = 0;
    
    int offset = length;
    
    byte pixel;
    for(i = 0; i < length; i++)
    {
    pixel = (byte)input[i];
    output[length - offset + bit3] = (byte)pixel;
    
    if(bit2 == height0){offset = length;}
    else{offset = length - bit1;}
    
    if(bit2 == height0){bit2 = 0; bit3 += 1;}
    bit2 +=1;
    bit1 = width0 * bit2;
    }
}

void GetIndex(char *name,int checkindex, int* filenum, int* offset)
{
    FILE *index;
    index = fopen(name,"rb");
    if(!index)
    {
     error("No puede abrir %s", name);
    }
    
    int count = ReadWord(index)*3;
    //printf("count %d\n",count);
    
    int k = 0;
    for (int n = 0; n < count; n += 3)
    {
        int i1 = ReadByte(index);
        int i2 = ReadWord(index);
        
        if (i1 != 255)
        {
           if(k == checkindex)
           {
             *filenum = i1;
             *offset = i2;
           }
           k++;
        }
        else
        {
            n -= 3;
        }
    }
    int i1 = ReadByte(index);
    int i2 = ReadWord(index);
    if(k == checkindex)
    {
      *filenum = i1;
      *offset = i2;
    }
    fclose(index);
}

static int Create_Walls(int wallindex,int paloffset,int id)
{
    int filenum = 0;
    int walloffset = 0;
    
    GetIndex("Datos/wtexels.idx",wallindex, &filenum, &walloffset);

    char string[16] = { 0 };
    //printf("filenum %d walloffset %d\n",filenum,walloffset);
    sprintf(string,"Datos/wtexels%d.bin",filenum);
    
    FILE *in, *out;
    in = fopen(string,"rb");
    if(!in)
    {
     error("No puede abrir %s", string);
    }

    fseek(in,1+walloffset,SEEK_SET);
    
    byte pixel = 0;
    int i;
    
    cache input;
    cache output;
    cache pngout;
    
    input = (byte*)malloc(4096);
    output = (byte*)malloc(4096);
    
    for(i = 0; i < 4096; i+=2)
    {
        pixel = ReadByte(in);
        input[i] = (byte)(pixel & 0xF);
        input[i+1] = (byte)(pixel >> 4 & 0xF);
    }
    
    rotate90andflip(input, output, 64, 64);

    fclose(in);

    int pngsize = 0;
    pngout = Png_Create(output, &pngsize,64, 64, paloffset);

    sprintf(string,"WTEXELS/WALL%03d",id/8);
    ZipAdd(hz, string, pngout, pngsize);
    
    free(input);
    free(output);
    free(pngout);
}

//void write_png_file(byte* input,int width, int height, int paloffset, int offsetx = 0, int offsety = 0);
static int startoffset = -1;
int Create_Sprites(int bitshapeindex,int paloffset,int id)
{
    /*int bitshapesoffset;
    FILE *in, *in2;
    in = fopen("bitshapes.bin","rb");
    if(!in)
    {
     printf("no puede abrir bitshapes.bin\n");
     system("PAUSE");
     return EXIT_SUCCESS;
    }*/
    int filenum;
    int bitshapesoffset;
    
    GetIndex("Datos/shapes.idx",bitshapeindex, &filenum, &bitshapesoffset);

    char string[16] = { 0 };
    //printf("filenum %d bitshapesoffset %d\n",filenum,bitshapesoffset);
    sprintf(string,"Datos/shapes%d.bin",filenum);
    
    FILE *in, *in2;
    in = fopen(string,"rb");
    if(!in)
    {
     error("No puede abrir %s", string);
    }

    fseek(in,bitshapesoffset,SEEK_SET);
    int stexelsindex = ReadUint(in);
    //printf("stexelsindex %d\n",stexelsindex);
    
    int stexelsoffset;
    GetIndex("Datos/stexels.idx",stexelsindex, &filenum, &stexelsoffset);
    stexelsoffset += 1;
    
    //printf("filenum %d stexelsoffset %d\n",filenum,stexelsoffset);
    sprintf(string,"Datos/stexels%d.bin",filenum);
    in2 = fopen(string,"rb");
    if(!in2)
    {
     error("No puede abrir %s", string);
    }
    //stexelsindex = 4+((stexelsoffset)/2);
    
    int bitcount = ReadWord(in)-4;
    int buffsize = ReadWord(in);//Solo usado en el juego original
    //printf("bitcount %d\n",bitcount);
    //printf("buffsize %d\n",buffsize);
    
    int x0 = ReadByte(in);
    int x1 = ReadByte(in);
    int y0 = ReadByte(in);
    int y1 = ReadByte(in);
    
    int width;
    int height;
    int heightOrg;
    int i14;
    
    int offsetx = 0;
    int offsety = 0;
    
    width = x1 - x0 + 1;
    heightOrg = y1 - y0 + 1;
    height= (i14 = y1 - y0 + 1) % 8 != 0 ? i14 / 8 + 1 : i14 / 8;
    height*=8;
    
    offsetx = (32-x0);
    offsety = (64-y0);

    //printf("x0 -> %d x1 -> %d\n",x0, x1);
    //printf("y0 -> %d y1 -> %d\n",y0, y1);
    
    //printf("width -> %d\n",width);
    //printf("height -> %d\n",height);
    
    cache input;
    cache output;
    cache pngout;
    
    input = (byte*)malloc(width*height);
    output = (byte*)malloc(width*height);
    
    byte pixel;
    byte pixel2;
    int pix = 0;
    int pix2 = 0;
    int count = 0;

    int transpix[0x10]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
    for(int a = 0; a < bitcount; a++)
    {
        pixel = ReadByte(in);
            
        for(int b = 0; b < 8; b++)
        {
            if((pixel & 1 << b) != 0)
            {
                fseek(in2,(stexelsoffset + pix),SEEK_SET);
                pixel2 = ReadByte(in2);

                if(pix2)
                {
                    input[count] = (byte)(pixel2 >> 4 & 0xF);//0xff;
                    transpix[(byte)input[count]] = -1;
                }
                else
                {
                    input[count] = (byte)(pixel2 & 0xF);//0xff;
                    transpix[(byte)input[count]] = -1;
                }
                if(pix2 == 1)
                {
                    pix++;
                    pix2 = 0;
                }
                else
                {
                    pix2++;
                }
            }
            else
            {
                input[count] = 0x7f;
            }
            count++;
        }
    }
    
    rotate90andflip(input, output, width, height);

    fclose(in);
    fclose(in2);
    
    //write_png_file(output,width,height,paloffset);
    
    int trans = -1;
    for(int i = 0; i < 0x10; i++)
    {
       if(transpix[i] != -1)
          trans = i;
    }
    int pngsize = 0;
    pngout = Png_Create(output, &pngsize,width,heightOrg, paloffset, offsetx, offsety, trans);

    sprintf(string,"STEXELS/STEX%03d",id/8);
    ZipAdd(hz, string, pngout, pngsize);
    
    free(input);
    free(output);
    free(pngout);
}

void ShowInfo()
{
    setcolor(0x07);printf("     ############");
    setcolor(0x0A);printf("(ERICK194)");
    setcolor(0x07);printf("#############\n"); 
    printf("     #     ORCS AND ELVES EXTRACTOR    #\n");
    printf("     # CREADO POR ERICK VASQUEZ GARCIA #\n");
    printf("     #      ES PARA ORCS AND ELVES     #\n");
    printf("     #         VERSION 1.0.16          #\n");
    printf("     #          MODO DE USO:           #\n");
    printf("     #  SOLO NECESITAS EL ARCHIVO JAR  #\n");
    printf("     ###################################\n");
    printf("\n");
}

void ExtraerDatos()
{
    hz = OpenZip(("orcs_and_elves_v_1.0.16.jar"),0);
    if(hz == NULL){error("No encuentra orcs_and_elves_v_1.0.16.jar");}
    SetUnzipBaseDir(hz,("Datos"));
    ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
    
    int i;
    FindZipItem(hz,("mappings.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("palettes.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("shapes0.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("shapes1.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("stexels0.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("stexels1.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("stexels2.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("stexels3.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("wtexels0.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("wtexels1.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("shapes.idx"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("stexels.idx"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    FindZipItem(hz,("wtexels.idx"),true,&i,&ze);
    UnzipItem(hz,i,ze.name);
    
    CloseZip(hz);
}

int main(int argc, char *argv[])
{
    ShowInfo();

    FILE *f1;
    
    f1 = fopen("Datos/mappings.bin","rb");
    if(!f1)
    {
     ExtraerDatos();
    }
    fclose(f1);
    
    
    f1 = fopen("Datos/mappings.bin","rb");
    if(!f1)
    {
     error("No puede abrir mappings.bin");
    }
    
    hz = CreateZip("OrcsAndElves.pk3",0);
    
    int count1 = ReadUint(f1);
    int count2 = ReadUint(f1);
    int count3 = ReadUint(f1)*2;
    int count4 = ReadUint(f1)*2;
    int i;
    int bitshapeindex;
    int paloffset;
    int wallindex;
    //printf("i1 %d, i2 %d, i3 %d, i4 %d\n",i1,i2,i3,i4);
    //printf("textures %d\n\n",count1);
    
    if (count1 > 40)
    {
     error("Error count1 supera el maximo de 40");
    }
    
    int start = 0;
    int end = (count1)-1;
    count1 = count1 * 8;
    for(i = 0; i < count1; i+=8)
    {
        PrintfPorcentaje(start,end,true, 9,"Extrayendo Texturas......");
        wallindex = ReadUint(f1); //printf("wallindex %d\n", wallindex);
        paloffset = 16 * ReadUint(f1); //printf("paloffset %d\n", paloffset);
        Create_Walls(wallindex, paloffset, i);
        start++;
    }
    
    if (count2 > 550)
    {
     error("Error count2 supera el maximo de 550");
    }
    
    start = 0;
    end = (count1)-1;
    count2 = count2 * 8;
    printf("Sprites %d\n\n",count2);
    for(int i = 0; i < count2; i+=8)
    {
        PrintfPorcentaje(start,end,true, 10,"Extrayendo Sprites.......");
        bitshapeindex = ReadUint(f1);//printf("bitshapesoffset %d\n",bitshapesoffset);
        paloffset = 16 * ReadUint(f1);//printf("paloffset %d\n", paloffset);
        Create_Sprites(bitshapeindex,paloffset,i);
        start++;
    }
    
    CloseZip(hz);
    fclose(f1);
    system("PAUSE");
    return EXIT_SUCCESS;
}
/*void write_png_file(byte* input,int width, int height, int paloffset, int offsetx, int offsety)
{
  png_structp png_ptr;
  png_infop info_ptr;
  png_colorp palette;
  cache* row_pointers;
  int bit_depth = 4;

  FILE *fp = fopen("PNG.png", "wb");
  if(!fp) abort();
  
  FILE *fpal = fopen ("palettes.bin","rb");
  if(!fpal)
  {
   error("No puede abrir palettes.bin");
  }

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();

  png_infop info = png_create_info_struct(png);
  if (!info) abort();

  if (setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  // Output is 8bit depth, RGBA format.
  png_set_IHDR(
    png,
    info,
    width, height,
    bit_depth,// 4, 8
    PNG_COLOR_TYPE_PALETTE,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );

  fseek(fpal,4 + (paloffset * 2),SEEK_SET);
  palette = (png_colorp) malloc((16*3)*png_sizeof(png_color));
     
  int gettrans = -1;
  for(int x = 0;x < 16;x++)
  {
    int Palbit = ReadWord(fpal);
    int R = (R = Palbit & 0x1F) << 3 | R >> 2;
    int G = (G = Palbit >> 5 & 0x3F) << 2 | G >> 4;
    int B = (B = Palbit >> 11 & 0x1F) << 3 | B >> 2;
    
    palette[x].red = R;
    palette[x].green = G;
    palette[x].blue = B;
    
    if(palette[x].red==0 && palette[x].green==255 && palette[x].blue==255)
      gettrans = x;
  }
     
  png_set_PLTE(png,info,palette,16);
     
  if(gettrans != -1)
  {
    png_byte trans[gettrans+1]; 
    for(int tr =0;tr < gettrans+1; tr++)
    {
      if(tr==gettrans){trans[tr]=0;}
      else {trans[tr]=255;}
    }
    png_set_tRNS(png,info,trans,gettrans+1,NULL);
  }

  png_write_info(png, info);
  
  if(offsetx !=0 && offsety !=0)
  {
      int offs[2];
    
      offs[0] = Swap32(offsetx);
      offs[1] = Swap32(offsety);
    
      png_write_chunk(png, (png_byte*)"grAb", (byte*)offs, 8);
  }

  png_set_packing(png);
  //image=data;
  byte inputdata;
     row_pointers=(png_bytep*)malloc(sizeof(png_bytep*)*height);
     for(int i=0;i < height;i++)
     {
     //printf("i=%d\n",i);
     row_pointers[i]=(png_bytep)malloc(width);
               //--4_bits--//
               for(int j=0;j < width; j++)
               {
               inputdata = *input;
               if(inputdata == 0x7f){inputdata = (byte)gettrans;}
               row_pointers[i][j] = inputdata;
               input++;
               }
     }

  png_write_image(png, row_pointers);
  png_write_end(png, NULL);

  for(int y = 0; y < height; y++) {
    free(row_pointers[y]);
  }
  free(row_pointers);
  
  fclose(fp);
  fclose(fpal);
  
  if (png && info)
  png_destroy_write_struct(&png, &info);
}*/
