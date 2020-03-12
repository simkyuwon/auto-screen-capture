#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>
#include <utility>
#include <map>
#include <queue>
#include <csignal>
#include <windows.h>
#include <direct.h>
#include <gdiplus.h>
#include <process.h>
#include <mmsystem.h>

#pragma comment(lib,"gdiplus")
#pragma comment(lib,"winmm.lib")

#define BYTE_CHK(n) (BYTE)((n)<0?0:((n)>=256?255:(n)))
#define RATE_CHK(n) ((n)<0?0:((n)>1?1:(n)))
#define SQRT(n) ((n)?sqrt(n):0)

using namespace std;
#pragma pack(push, 1)
typedef struct PIXEL_ARGB{
	BYTE B;
	BYTE G;
	BYTE R;
	BYTE A;
}pixelARGB;
#pragma pack(pop)

typedef struct EDGE_VAR{
	queue<double> avg;
	double sum;
	double expsum;
	double ref;
}edgeVar;

enum Command{
	CMD_HELP = 1,
	CMD_QUIT,
	CMD_RESTART,
	CMD_SETDELAY,
	CMD_SETTIMER,
	CMD_SETALRM,
	CMD_SETDEBUG,
	CMD_SETREFVAL,
	CMD_SETREFRATE,
	CMD_SETSENS,
	CMD_SETSIZE,
	CMD_CLEAR,
	CMD_INFO,
	CMD_HIDE,
};

static map <string, int> cmd;

int gaussianFilter[5][5] = {
{1, 4, 6, 4, 1},
{4, 16, 24, 16, 4},
{6, 24, 36, 24, 6},
{4, 16, 24, 16, 4},
{1, 4, 6, 4, 1}};

int sobelMask[2][3][3] = {
{{-1, 0, 1},
{-2, 0, 2},
{-1, 0, 1}},
{{1, 2, 1},
{0, 0, 0,},
{-1, -2, -1}}};

Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;
CLSID clsid;
HANDLE hThread;
DWORD threadID;
int MonitorX = GetSystemMetrics(SM_XVIRTUALSCREEN);
int MonitorY = GetSystemMetrics(SM_YVIRTUALSCREEN);
int Height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
int Width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
int resizeHeight = Height/2;
int resizeWidth = Width/2;
int maximumSaveDelay = 180, minimumSaveDelay = 10;//sec
int timerDelay = 5;//fps
int deleteDelay = 3;//day
int alarmDelay = 180;
int alarmTimer;
double referenceVal = 0.001;
double referenceRate = 0.01;
double sensitivity = 0.1;
char path[128];
time_t prev_time = 0;
bool debug = false;
BYTE flags = 0x00;
int sectionSize = 10;
edgeVar *edgeVarArr;
char *exePath;

void CALLBACK routine(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime);
void exitHandler(int signum);
void init();
void initSetting();
bool dirExists(const string& dirName_in);
void ConvertCtoWC(const char *str, wchar_t *wstr);
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool gdiscreen(const char* filename, bool maximum, bool minimum);
unsigned int WINAPI keyboardInput(void *args);
HWND GetConsoleHwnd(void);
double absDiff(pixelARGB A, pixelARGB B);

int main(int argc, char* argv[]){
	char buf[256];
	int key = 0;
	MSG msg;

	exePath = argv[0];
	sprintf(buf,"%s\\..\\screenCapture", argv[0]);
	_fullpath(path, buf, sizeof(path));

	init();

	while(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0)
			break; 
	}

	KillTimer(NULL, 0);

	delete []edgeVarArr;
	delete []exePath;

	Gdiplus::GdiplusShutdown(gdiplusToken);

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
	BYTE *imgArr, *grayArr, *edgeArr;

	imgArr = new BYTE[resizeWidth * resizeHeight];
	grayArr = new BYTE[Width * Height];
	edgeArr = new BYTE[resizeWidth * resizeHeight];
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(NULL);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, MonitorX, MonitorY, SRCCOPY);

		Bitmap *origin = new Bitmap(membit, NULL);
		Bitmap *bitmap = new Bitmap(resizeWidth, resizeHeight, PixelFormat32bppARGB);

		BitmapData *bitmapData = new BitmapData;
		Rect rect(0, 0, Width, Height);
		origin->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, bitmapData);

		pixelARGB *pixels = (pixelARGB *)(bitmapData->Scan0);

		for(int j = 0;j < Height; j++){//grayscale
			for(int i = 0;i < Width; i++){
				int idx = j * Width + i;
				grayArr[idx] = BYTE_CHK(0.2126 * pixels[idx].R + 0.7152 * pixels[idx].G + 0.0722 * pixels[idx].B);
			}
		}

		for(int j = 0; j < Height; j += 2){//gaussian filter
			for(int i = 0; i < Width; i += 2){
				int idx = j * Width + i;
				int Y = 0, cnt = 0;
				for(int y = (j>1?0:(2-j)); y <= (j+2>=Height?(Height-j+1):4); y++){
					for(int x = (i>1?0:(2-i)); x <= (i+2>=Width?(Width-i+1):4); x++){
						int filterIdx = idx + (y - 2) * Width + (x - 2);
						cnt += gaussianFilter[y][x];
						Y += grayArr[filterIdx] * gaussianFilter[y][x];
					}
				}
				imgArr[(j * resizeWidth + i) / 2] = BYTE_CHK(Y / cnt);
			}
		}
		
		origin->UnlockBits(bitmapData);

		Rect arrRect(0, 0, resizeWidth, resizeHeight);
		bitmap->LockBits(&arrRect, ImageLockModeWrite, PixelFormat32bppARGB, bitmapData);
		pixels = (pixelARGB *)(bitmapData->Scan0);

		for(int j = 1; j < resizeHeight - 1; j++){//edge detection
			for(int i = 1; i < resizeWidth - 1; i++){
				int idx = j * resizeWidth + i;
				edgeArr[idx] = 0;
				int sobelX = 0, sobelY = 0;
				for(int y = 0; y < 3; y++){
					for(int x = 0; x < 3; x++){
						int maskIdx = idx + (y - 1) * resizeWidth + (x - 1);
						sobelX += sobelMask[0][y][x] * imgArr[maskIdx];
						sobelY += sobelMask[1][y][x] * imgArr[maskIdx];
					}
				}
				edgeArr[idx] = BYTE_CHK(SQRT(sobelX * sobelX + sobelY * sobelY) / 5.6);
				if(debug)
					pixels[idx] = {BYTE_CHK(edgeArr[idx]), 0, 0, 0};
			}
		}

		double rate = 0;
		int sectionW, sectionH = sectionSize;
		for(int j = 0; j < resizeHeight; j += sectionSize){//change detection
			if(j + sectionSize >= resizeHeight)
				sectionH = resizeHeight - j;
			for(int i = 0; i < resizeWidth; i += sectionSize){
				if(i + sectionSize >= resizeWidth)
					sectionW = resizeWidth - i;
				else
					sectionW = sectionSize;
				int idx = j * resizeWidth + i;
				double avg = 0;
				for(int y = 0; y < sectionH; y++){
					for(int x = 0; x < sectionW; x++){
						avg += edgeArr[idx + y * resizeWidth + x];
					}
				}
				avg /= sectionH*sectionW;
				int edgeIdx = (j / sectionSize) * (resizeWidth / sectionSize + (resizeWidth % sectionSize?1:0)) + (i / sectionSize);
				edgeVarArr[edgeIdx].avg.push(avg);
				edgeVarArr[edgeIdx].sum += avg;
				edgeVarArr[edgeIdx].expsum += avg * avg;
				if(edgeVarArr[edgeIdx].avg.size() > min(minimumSaveDelay, 10) * timerDelay){
					double popAvg = edgeVarArr[edgeIdx].avg.front();
					edgeVarArr[edgeIdx].sum -= popAvg;
					edgeVarArr[edgeIdx].expsum -= popAvg * popAvg;
					edgeVarArr[edgeIdx].avg.pop();
				}

				double edgeS = edgeVarArr[edgeIdx].expsum / edgeVarArr[edgeIdx].avg.size() - pow(edgeVarArr[edgeIdx].sum / edgeVarArr[edgeIdx].avg.size(), 2);
				if(edgeS > 0){
					edgeS = sqrt(edgeS);
					edgeS = (edgeS * 2) / 255;
				}

				if(edgeS > edgeVarArr[edgeIdx].ref + referenceVal)
					rate += sectionW * sectionH;

				if(debug){
					for(int y = 0; y < sectionH; y++){
						for(int x = 0; x < sectionW; x++){
							pixels[idx + y * resizeWidth + x].R = BYTE_CHK(avg);
							pixels[idx + y * resizeWidth + x].G = (edgeS > edgeVarArr[edgeIdx].ref + referenceVal)?128:0;
						}
					}
				}
				edgeVarArr[edgeIdx].ref = edgeVarArr[edgeIdx].ref * (1 - sensitivity) + edgeS * sensitivity;
			}
		}

		bitmap->UnlockBits(bitmapData);
		delete bitmapData;

		if(rate/(resizeWidth * resizeHeight) > referenceRate)
			flags |= 0x01;

		if(maximum)
			flags |= 0x02;

		if(minimum && (flags & 0x03)){//save image
			if(flags & 0x01){
				sprintf(buf, "%s(event).jpeg", filename);
				if(!(flags & 0x40)){
					PlaySound((LPCSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ASYNC | SND_ALIAS_ID);
					MessageBox(NULL, "change detection", "autoScreenCapture", MB_OK | MB_ICONASTERISK | MB_TOPMOST);
					alarmTimer = alarmDelay * timerDelay;
					flags |= 0x40;
				}
			}
			else{
				if(alarmTimer == 0) flags &= 0xBF;
				sprintf(buf, "%s.jpeg", filename);
			}
			ConvertCtoWC(buf, WCbuf);
			origin->Save(WCbuf, &clsid, NULL);
			if(debug){
				sprintf(buf, "%s(debug).jpeg", filename);
				ConvertCtoWC(buf, WCbuf);
				bitmap->Save(WCbuf, &clsid, NULL);
			}
			save = true;
			flags &= 0xFC;
		}

		delete origin;
		delete bitmap;
		DeleteObject(memdc);
		DeleteObject(membit);
		::ReleaseDC(NULL, scrdc);
	}

	delete []grayArr;
	delete []imgArr;
	delete []edgeArr;
	
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

void CALLBACK routine(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime){
	if(alarmTimer > 0) alarmTimer--;
	if(flags & 0x80)return;
	flags |= 0x80;
	
	char filename[128];
	char foldername[128];
	char buf[256];
	time_t curr_time;
	struct tm *curr_tm, *tmp_tm;

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
/*			WinExec(exePath, SW_HIDE);//restart
			PostQuitMessage(WM_QUIT);*/
		}
	}
	strftime(filename, sizeof(filename), "%H-%M-%S", curr_tm);
	sprintf(buf, "%s\\%s\\%s", path, foldername, filename);

	if(gdiscreen(buf, difftime(curr_time, prev_time) >= maximumSaveDelay, difftime(curr_time, prev_time) >= minimumSaveDelay))
		time(&prev_time);
	flags &= 0x7F;
}

void exitHandler(int signum){
	MessageBox(NULL, "program is terminated", "autoScreenCapture", MB_OK|MB_ICONERROR);	
}

void init(){
	cmd["help"] = CMD_HELP;//init command map
	cmd["quit"] = CMD_QUIT;
	cmd["restart"] = CMD_RESTART;
	cmd["setDelay"] = CMD_SETDELAY;
	cmd["setTimer"] = CMD_SETTIMER;
	cmd["setAlarm"] = CMD_SETALRM;
	cmd["setDebug"] = CMD_SETDEBUG;
	cmd["setRefVal"] = CMD_SETREFVAL;
	cmd["setRefRate"] = CMD_SETREFRATE;
	cmd["setSens"] = CMD_SETSENS;
	cmd["setSize"] = CMD_SETSIZE;
	cmd["clear"] = CMD_CLEAR;
	cmd["info"] = CMD_INFO;
	cmd["hide"] = CMD_HIDE;

	if(!dirExists(path))
		mkdir(path);

	AllocConsole();//init concole
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	initSetting();

	HWND hwndFound;
	char pszNewWindowTitle[1024];
	char pszOldWindowTitle[1024];

	GetConsoleTitle(pszOldWindowTitle, 1024);
	
	SetConsoleTitle("autoScreenCapture_tmp");
	
	Sleep(40);

	for(hwndFound = FindWindow(NULL, pszOldWindowTitle); hwndFound != NULL; hwndFound = FindWindow(NULL, pszOldWindowTitle)){//same program exit
		HANDLE hProcess;
		DWORD lpdwProcessId;
		GetWindowThreadProcessId(hwndFound, &lpdwProcessId);
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, lpdwProcessId);
		TerminateProcess(hProcess, 0);
	}
	
	SetConsoleTitle(pszOldWindowTitle);

	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	GetEncoderClsid(L"image/jpeg", &clsid);

	edgeVarArr = new edgeVar[(resizeWidth/sectionSize + (resizeWidth%sectionSize?1:0)) * (resizeHeight/sectionSize + (resizeHeight%sectionSize?1:0))];

	SetTimer(NULL, 0, (int)1000/timerDelay, (TIMERPROC)&routine);

	signal(SIGABRT, exitHandler);

	alarmTimer = alarmDelay * timerDelay;
	flags |= 0x40;

	hThread = (HANDLE)_beginthreadex(NULL, 0, keyboardInput, NULL, 0, (unsigned*)&threadID);
}

void initSetting(){
	FILE *pFile;
	char tmp = '\0';
	pFile = fopen("setting", "r");
	if(pFile != NULL){
		fseek(pFile, 0, SEEK_SET);
		if(fscanf(pFile, "%9d %9d %9d %9d %9d %lf %lf %lf %c", &timerDelay, &minimumSaveDelay, &maximumSaveDelay, &sectionSize, &alarmDelay, &referenceVal, &referenceRate, &sensitivity, &tmp) != 9){
			fclose(pFile);
			remove("setting");
			tmp = '\0';
		}
		else{
			debug = (tmp=='T')?true:false;
		}
	}
	if(!tmp){//make setting file
		pFile = fopen("setting", "w");
		fprintf(pFile, "%9d %9d %9d %9d %9d %1.7lf %1.7lf %1.7lf F", timerDelay, minimumSaveDelay, maximumSaveDelay, sectionSize, alarmDelay, referenceVal, referenceRate, sensitivity);
	}
	fclose(pFile);
}

unsigned int WINAPI keyboardInput(void *args){//keyboard input thread
	char input[256];
	char buf[256];
	int inputIdx, bufIdx;
	while(true){
		cin.getline(input, sizeof(input));
		sscanf(input, "%s", buf);
		FILE *pFile;
		pFile = fopen("setting", "r+");
		switch(cmd[buf]){
			case CMD_HELP:
				printf("help : show command\n");
				printf("quit : quit program\n");
				printf("restart : restart program\n");
				printf("hide : hide the window\n");
				printf("setDelay %%d %%d : set minimum, maximum save delay(sec)\n");
				printf("setTimer %%d : set timer delay(fps)\n");
				printf("setAlarm %%d : set alarm delay(sec)\n");
				printf("setDebug (T|F) : set debug mode\n");
				if(debug){
					printf("setRefVal %%lf : set reference value\n");
					printf("setRefRate %%lf : set reference rate\n");	
					printf("setSens %%lf : set sensitivity\n");	
					printf("setSize %%d : set section size\n");
				}
				printf("clear : delete save image\n");
				printf("info : show information\n");
				break;
			case CMD_HIDE:
				ShowWindow(GetConsoleHwnd(), SW_HIDE);
				break;
			case CMD_RESTART:
				WinExec(exePath, SW_SHOW);
			case CMD_QUIT:
				fclose(pFile);
				return 0;
			case CMD_SETDELAY:{
				int minimum, maximum;
				if(sscanf(input, "%*s %d %d", &minimum, &maximum) != 2){
					printf("input error\n");
					break;
				}
				if(minimum == 0)
					minimum = minimumSaveDelay;
				if(maximum == 0)
					maximum = maximumSaveDelay;
				if(minimum > maximum){
					printf("error : (%d)minimumSaveDelay > (%d)maximumSaveDelay\n", minimum, maximum);
					break;
				}
				else{
					minimumSaveDelay = minimum;
					maximumSaveDelay = maximum;
				}
				printf("minimumSaveDelay : %dsec, maximumSaveDelay : %dsec\n", minimum, maximum); 
				fseek(pFile, 10, SEEK_SET);
				fprintf(pFile, "%9d %9d ", minimumSaveDelay, maximumSaveDelay);
				break;}
			case CMD_SETTIMER:{
				int timer;
				if(sscanf(input, "%*s %d", &timer) != 1){
					printf("input error\n");
					break; 
				}
				if(timer < 1)
					timer = 1;
				else if(timer > 20)
					timer = 20;
				alarmTimer = alarmDelay * timer / timerDelay;
				timerDelay = timer;
				printf("timerDelay : %dfps\n", timerDelay);
				fseek(pFile, 0, SEEK_SET);
				fprintf(pFile, "%9d ", timerDelay);
				fclose(pFile);
				WinExec(exePath, SW_SHOW);
				return 0;}
			case CMD_SETALRM:{
				int alarm;
				if(sscanf(input, "%*s %d", &alarm) != 1){
					printf("input error\n");
					break; 
				}
				if(alarm < 1)
					alarm = 1;
				alarmDelay = alarm;
				if(alarmTimer > alarmDelay * timerDelay) alarmTimer = alarmDelay * timerDelay;
				printf("alarmDelay : %dsec\n", alarm);
				fseek(pFile, 40, SEEK_SET);
				fprintf(pFile, "%9d ", alarmDelay);
				break;}
			case CMD_SETDEBUG:{
				char tmp;
				if(sscanf(input, "%*s %c", &tmp) != 1 || (tmp != 'T' && tmp != 'F')){
					printf("input error\n");
					break;
				}
				debug = (tmp=='T')?true:false;
				printf("Debug : %c\n", tmp);
				fseek(pFile, 80, SEEK_SET);
				fprintf(pFile, "%c", tmp);
				break;}
			case CMD_SETREFVAL:{
				double refVal;
				if(sscanf(input, "%*s %lf", &refVal) != 1){
					printf("input error\n");
					break;
				}
				referenceVal = RATE_CHK(refVal);
				printf("reference value : %lf\n", referenceVal);
				fseek(pFile, 50, SEEK_SET);
				fprintf(pFile, "%1.7lf ", refVal);
				break;}	
			case CMD_SETREFRATE:{
				double refRate;
				if(sscanf(input, "%*s %lf", &refRate) != 1){
					printf("input error\n");
					break;
				}
				referenceRate = RATE_CHK(refRate);
				printf("reference Rate : %lf\n", referenceRate);
				fseek(pFile, 60, SEEK_SET);
				fprintf(pFile, "%1.7lf ", refRate);
				break;}	
			case CMD_SETSENS:{
				double sens;
				if(sscanf(input, "%*s %lf", &sens) != 1){
					printf("input error\n");
					break;
				}
				sensitivity = RATE_CHK(sens);
				printf("sensitivity : %lf\n", sensitivity);
				fseek(pFile, 70, SEEK_SET);
				fprintf(pFile, "%1.7lf ", sensitivity);
				break;}
			case CMD_SETSIZE:{
				int size;
				if(sscanf(input, "%*s %d", &size) != 1){
					printf("input error\n");
					break;
				}
				if(size < 1)size = 1;
				else if(size > min(resizeWidth, resizeHeight))size = min(resizeWidth, resizeHeight);
				sectionSize = size;
				printf("section size : %d\n", sectionSize);
				fseek(pFile, 30, SEEK_SET);
				fprintf(pFile, "%9d ", size);
				fclose(pFile);
				WinExec(exePath, SW_SHOW);
				return 0;}
			case CMD_CLEAR:
				if(dirExists(path)){
					char sysBuf[256];
					sprintf(sysBuf, "rmdir /s /q %s", path);//delete directory
					system(sysBuf);
					mkdir(path);
					printf("saved image has been deleted\n");
				}
				break;
			case CMD_INFO:
				printf("Width X Height : %d %d\n", Width, Height);
				printf("screen capture : %dfps\n", timerDelay);
				printf("save delay : %d ~ %dsec\n", minimumSaveDelay, maximumSaveDelay);
				printf("alarm delay : %dsec\n", alarmDelay);
				if(debug){
					printf("section size : %d\n", sectionSize);
					printf("reference value : %lf\n", referenceVal);
					printf("reference rate : %lf\n", referenceRate);
					printf("sensitivity : %lf\n", sensitivity);
				}
				break;
			default:
				printf("command not found\nhelp : show command\n");
				break;
		}
		fclose(pFile);
	}
}

HWND GetConsoleHwnd(void){
	char WindowTitle[1024];
	GetConsoleTitle(WindowTitle, 1024);
	return FindWindow(NULL, WindowTitle);
}

double absDiff(pixelARGB A, pixelARGB B){
	double ret = (A.R - B.R) * (A.R - B.R) + (A.G - B.G) * (A.G - B.G) + (A.B - B.B) * (A.B - B.B);
	if(ret) ret = sqrt(ret);
	return ret;
}
