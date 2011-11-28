#include "stdafx.h"
#include "bitmap_utils.hpp"
#include "utils.hpp"

bool save_bmp_mono(const char *filename, uint8_t *ptr, int width, int height)
{
	uint8_t *buf = new uint8_t[width*height*4];
	uint8_t *src = ptr;
	uint32_t *dst = (uint32_t *)buf;
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			uint8_t b = *src++;
			uint32_t d = b << 24 | b << 16 | b << 8 | b;
			*dst++ = d;
		}
	}
	bool res = save_bmp32(filename, buf, width, height);
	SAFE_ADELETE(buf);
	return res;
}

bool save_bmp32(const char *filename, uint8_t *ptr, int width, int height)
{
	FILE *f = fopen(filename, "wb");
	if (!f)
		return false;

#define BITMAP_SIGNATURE 'MB'

#pragma pack(push, 1)

	typedef struct {
		unsigned short int Signature;
		unsigned int Size;
		unsigned int Reserved;
		unsigned int BitsOffset;
	} BITMAP_FILEHEADER;

#define BITMAP_FILEHEADER_SIZE 14

	typedef struct {
		unsigned int HeaderSize;
		int Width;
		int Height;
		unsigned short int Planes;
		unsigned short int BitCount;
		unsigned int Compression;
		unsigned int SizeImage;
		int PelsPerMeterX;
		int PelsPerMeterY;
		unsigned int ClrUsed;
		unsigned int ClrImportant;
		unsigned int RedMask;
		unsigned int GreenMask;
		unsigned int BlueMask;
		unsigned int AlphaMask;
		unsigned int CsType;
		unsigned int Endpoints[9]; // see http://msdn2.microsoft.com/en-us/library/ms536569.aspx
		unsigned int GammaRed;
		unsigned int GammaGreen;
		unsigned int GammaBlue;
	} BITMAP_HEADER;

#pragma pack(pop)

	BITMAP_FILEHEADER fh;
	BITMAP_HEADER h;

	ZeroMemory(&fh, sizeof(fh));
	ZeroMemory(&h, sizeof(h));

	fh.Signature = BITMAP_SIGNATURE;
	fh.Size = width * height * 4;
	fh.BitsOffset = sizeof(BITMAP_HEADER) + sizeof(BITMAP_FILEHEADER);

	h.HeaderSize = sizeof(h);
	h.Planes = 1;
	h.BitCount = 32;
	h.Width = width;
	h.Height = height;
	h.SizeImage = width * height * 4;
	h.PelsPerMeterX = h.PelsPerMeterY = 3780;
	fwrite(&fh, sizeof(fh), 1, f);
	fwrite(&h, sizeof(h), 1, f);
	// flip the image
	for (int i = 0; i < height; ++i)
		fwrite(ptr + (height - i - 1) * width * 4, width * 4, 1, f);
	fclose(f);

	return true;
}
