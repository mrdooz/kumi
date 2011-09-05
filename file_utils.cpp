#include "stdafx.h"
#include "file_utils.hpp"
#include "utils.hpp"

string replace_extension(const char *org, const char *new_ext) {
	const char *dot = strrchr(org, '.');
	if (!dot)
		return org;
	return string(org, dot - org + 1) + new_ext;
}

void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext) {
	char drive_buf[_MAX_DRIVE];
	char dir_buf[_MAX_DIR];
	char fname_buf[_MAX_FNAME];
	char ext_buf[_MAX_EXT];
	_splitpath(path, drive_buf, dir_buf, fname_buf, ext_buf);
	if (drive) *drive = drive_buf;
	if (dir) *dir = dir_buf;
	if (fname) *fname = fname_buf;
	if (ext) *ext = ext_buf;
}

bool load_file(const char *filename, void **buf, size_t *size)
{
	ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
	if (h.handle() == INVALID_HANDLE_VALUE)
		return false;

	*size = GetFileSize(h, NULL);
	*buf = new uint8_t[*size];
	DWORD res;
	if (!ReadFile(h, *buf, *size, &res, NULL))
		return false;

	return true;
}

bool file_exists(const char *filename)
{
	if (_access(filename, 0) != 0)
		return false;

	struct _stat status;
	if (_stat(filename, &status) != 0)
		return false;

	return !!(status.st_mode & _S_IFREG);
}

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
