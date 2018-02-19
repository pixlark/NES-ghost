# Independent
src=src/main.cc src/astar.cc
out=-o bin/nes -Wno-write-strings
opts=-std=c++11
dyn_libs=-lSDL2 -lrender

# Windows
win_incl_dirs=-I"G:\.minlib\SDL2-2.0.7\x86_64-w64-mingw32\include" -I"G:\.libraries\GLEW\include" -I"G:\C++\2018\gl-backend\src"
win_lib_dirs =-L"G:\.minlib\SDL2-2.0.7\x86_64-w64-mingw32\lib" -L"G:\.minlib\glew-2.1.0\lib" -L"G:\C++\2018\gl-backend\bin"

# Filled options
# TODO(pixlark): nix support
win_full=$(out) $(opts) $(src) $(win_incl_dirs) $(win_lib_dirs) $(dyn_libs)

win:
	@echo Building Release...
	g++ $(win_full)
	cp "G:\C++\2018\gl-backend\bin\render.dll" \
   "bin\render.dll"

wing:
	@echo Building Debug...
	g++ -g $(win_full)
	cp "G:\C++\2018\gl-backend\bin\render.dll" "bin\render.dll"
