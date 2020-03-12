autoScreenCapture_exe :
	x86_64-w64-mingw32-g++ -mwindows main.cpp -o autoScreenCapture_64bit.exe -lgdiplus -lwinmm -static
	i686-w64-mingw32-g++ -mwindows main.cpp -o autoScreenCapture_32bit.exe -lgdiplus -lwinmm -static

clean :
	rm autoScreenCapture*
