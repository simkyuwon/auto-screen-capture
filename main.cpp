#include <iostream>
#include <ctime>
#include <cstring>
#include <utility>
#include <stack>
#include <windows.h>
#include <direct.h>
#include <gdiplus.h>

#pragma comment(lib,"gdiplus")

#define BYTE_CHK(n) (n<0?0:(n>=256?255:n))
#define absDiff(a,b) (a>b?a-b:b-a)

using namespace std;
#pragma pack(push, 1)
typedef struct PIXEL_ARGB{
	BYTE B;
	BYTE G;
	BYTE R;
	BYTE A;
}pixelARGB;
#pragma pack(pop)

int Height = GetSystemMetrics(SM_CYSCREEN);
int Width = GetSystemMetrics(SM_CXSCREEN);
int resizeHeight = Height/2;
int resizeWidth = Width/2;
	
bool dirExists(const string& dirName_in);
void ConvertCtoWC(const char *str, wchar_t *wstr);
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool gdiscreen(const char* filename, bool maximum, bool minimum);
int bfs();

BYTE *imgArr, *prevImgArr, *diffImgArr;

int main(int argc, char* argv[]){
	AllocConsole();//init concole
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	imgArr = new BYTE[resizeWidth * resizeHeight];
	prevImgArr = new BYTE[resizeWidth * resizeHeight];
	diffImgArr = new BYTE[resizeWidth * resizeHeight];
	memset(prevImgArr, 0, sizeof(BYTE) * resizeWidth * resizeHeight);

	char filename[128];
	char foldername[128];
	char path[128];
	char buf[256];
	time_t curr_time;
	time_t prev_time = 0;
	struct tm *curr_tm, *tmp_tm;
	int maximumSaveDelay = 10, minimumSaveDelay = 3;//sec
	int deleteDelay = 3;//day

	sprintf(buf,"%s\\..\\screenCapture", argv[0]);
	_fullpath(path, buf, sizeof(path));

	if(!dirExists(path))
		mkdir(path);
	while(true){
		time(&curr_time);
		curr_tm = localtime(&curr_time);
		strftime(foldername, sizeof(foldername), "%y%m%d", curr_tm);
		sprintf(buf, "%s\\%s", path, foldername);
		if(!dirExists(buf)){
			mkdir(buf);
	
			time_t tmp_time = curr_time - 86400 * deleteDelay;
			tmp_tm = localtime(&tmp_time);
			strftime(foldername, sizeof(foldername), "%y%m%d", tmp_tm);
			sprintf(buf, "%s\\%s", path, foldername);
			if(dirExists(buf)){
				sprintf(buf, "rmdir /s /q %s\\%s", path, foldername);//delete directory
				system(buf);
			}
		}
		strftime(filename, sizeof(filename), "%H-%M-%S", curr_tm);
		sprintf(buf, "%s\\%s\\%s", path, foldername, filename);
		if(gdiscreen(buf, difftime(curr_time, prev_time) >= maximumSaveDelay, difftime(curr_time, prev_time) >= minimumSaveDelay))
			time(&prev_time);
	}

	delete[] imgArr;
	delete[] prevImgArr;

	return 0;
}

void ConvertCtoWC(const char *str, wchar_t *wstr){
	MultiByteToWideChar(CP_ACP, 0, str, strlen(str)+1, wstr, MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, NULL));
}

bool dirExists(const string& dirName_in){
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;	//something is wrong with your path!

	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;	 // this is a directory!

	return false;		// this is not a directory!
}

bool gdiscreen(const char* filename, bool maximum, bool minimum){
	using namespace Gdiplus;
	char buf[256];
	wchar_t WCbuf[256];
	bool save = false;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(0);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);

		Bitmap origin(membit, NULL);
		Bitmap bitmap(resizeWidth, resizeHeight, PixelFormat32bppARGB);

		//grayscale		
		auto *bitmapData = new BitmapData;
		Rect rect(0, 0, Width, Height);
		origin.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, bitmapData);

		pixelARGB *pixels = (pixelARGB *)(bitmapData->Scan0);
		pixelARGB pixel;

		for(int j = 0; j < Height; j += 2){
			for(int i = 0; i < Width; i += 2){
				pixel = pixels[j * Width + i];
				imgArr[(j * resizeWidth + i) / 2] = BYTE_CHK(0.2126 * pixel.R + 0.7152 * pixel.G + 0.0722 * pixel.B);
			}
		}

		bitmap.UnlockBits(bitmapData);

		Rect arrRect(0, 0, resizeWidth, resizeHeight);
		bitmap.LockBits(&arrRect, ImageLockModeWrite, PixelFormat32bppARGB, bitmapData);

		pixels = (pixelARGB *)(bitmapData->Scan0);
		for(int j = 0; j < resizeHeight; j++){
			for(int i = 0; i < resizeWidth ; i++){
				int idx = j * resizeWidth + i;
				diffImgArr[idx] = absDiff(imgArr[idx], prevImgArr[idx]);
				pixels[idx] = {diffImgArr[idx], diffImgArr[idx], diffImgArr[idx], 0};
				prevImgArr[idx] = imgArr[idx];
			}
		}
		
		int maxSize = bfs();

		bitmap.UnlockBits(bitmapData);
		delete(bitmapData);
		
		if(minimum && (maximum || maxSize > resizeWidth * resizeHeight * 0.01)){
			CLSID clsid;
			GetEncoderClsid(L"image/jpeg", &clsid);
			sprintf(buf, "%s(origin).jpeg", filename);
			ConvertCtoWC(buf, WCbuf);
			origin.Save(WCbuf, &clsid, NULL);
			sprintf(buf, "%s.jpeg", filename);
			ConvertCtoWC(buf, WCbuf);
			bitmap.Save(WCbuf, &clsid, NULL);
			save = true;
		}
		DeleteObject(memdc);
		DeleteObject(membit);
		::ReleaseDC(0,scrdc);
	}
	GdiplusShutdown(gdiplusToken);

	return save;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid){
	using namespace Gdiplus;
	UINT	num = 0;	// number of image encoders
	UINT	size = 0;	// size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if(size == 0)
		return -1;	// Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
		return -1;	// Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;	// Success
		}		
	}

	free(pImageCodecInfo);
	return 0;
}

int bfs(){
	int *arr = new int[resizeHeight * resizeWidth];
	int cnt = 0, size, maxSize = 0;
	memset(arr, 0, sizeof(BYTE) * resizeHeight * resizeWidth);
	stack<int> s;
	for(int j = 0;j < resizeHeight; j++){
		for(int i = 0;i < resizeWidth; i++){
			int idx = j * resizeWidth + i;
			if(arr[idx] == 0){
				if(diffImgArr[idx] == 0){
					arr[idx] = -1;
					continue;	
				}
				size = 0;
				arr[idx] = ++cnt;
				s.push(idx);
				while(!s.empty()){
					idx = s.top();
					s.pop();
					if(diffImgArr[idx] == 0)continue;
					size++;
					if(idx >= 1 && arr[idx - 1] == 0){
						arr[idx - 1] = cnt;
						s.push(idx - 1);
					}
					if(idx + 1 < resizeHeight * resizeWidth && arr[idx + 1] == 0){
						arr[idx + 1] = cnt;
						s.push(idx + 1);
					}
					if(idx >= resizeWidth && arr[idx - resizeWidth] == 0){
						arr[idx - resizeWidth] = cnt;
						s.push(idx - resizeWidth);
					}
					if(idx + resizeWidth < resizeHeight * resizeWidth && arr[idx + resizeWidth] == 0){
						arr[idx + resizeWidth] = cnt;
						s.push(idx + resizeWidth);
					}
				}
				if(maxSize < size)
					maxSize = size;
				return maxSize;
			}
		}
	}
	return maxSize;
}
