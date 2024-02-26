


OBJS	= main.o steam.o
SOURCE	= main.cpp steam.cpp
OBJS_EOS = main.o epic.o
SOURCE_EOS = main.cpp epic.cpp
OBJS_GOG = main.o steam.o
SOURCE_GOG = main.cpp steam.cpp
HEADER	= st_common.h
OUT	= wrath-steam
OUT_EOS = wrath-epic
OUT_GOG = wrath-gog
CC	 = g++ -std=c++11
CC_WIN= i686-w64-mingw32-g++ -std=c++11 -static-libgcc -static-libstdc++ -mwindows
CC_CLANG= clang++ --target=i386-windows-msvc
FLAGS	 = -g -c -Wall
LFLAGS	 = '-Wl,-rpath=$$ORIGIN'



.DEFAULT_TARGET: steam
.PHONY: epic gog steam

steam:
	$(CC) -DSTEAM $(SOURCE) -o $(OUT) $(LFLAGS) -I steam libsteam_api.so

epic:
	$(CC) -DEPIC $(SOURCE_EOS) -o $(OUT_EOS) $(LFLAGS) -I epic libEOSSDK-Linux-Shipping.so

#gog:
#	$(CC) -DGOG $(SOURCE_GOG) -o $(OUT_GOG) $(LFLAGS) -I gog GalaxySteamWrapper.so

steam-win:
	$(CC_WIN) -DSTEAM $(SOURCE) -o $(OUT).exe $(LFLAGS) -I steam steam_api.lib

epic-win:
	$(CC_WIN) -DEPIC $(SOURCE_EOS) -o $(OUT_EOS).exe $(LFLAGS) -I epic EOSSDK-Win32-Shipping.lib

gog-win:
	$(CC_CLANG) -DSTEAM -DGOG $(SOURCE_GOG) -o $(OUT_GOG).exe $(LFLAGS) -I gog GalaxySteamWrapper.lib

clean:
	rm -f $(OBJS) $(OBJS_EOS) $(OBJS_GOG) $(OUT) $(OUT_EOS) $(OUT_GOG) $(OUT).exe $(OUT_EOS).exe $(OUT_GOG).exe
