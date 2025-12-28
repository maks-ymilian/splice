@echo off
mkdir build
cd build && cmake .. -DCURL_USE_LIBPSL=OFF -DCURL_USE_SCHANNEL=ON -DBUILD_SHARED_LIBS=OFF && cmake --build . --config Release
pause