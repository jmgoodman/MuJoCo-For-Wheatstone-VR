# notes
for now, the display doesn't tile both monitors like it should
it merely tiles the top half of one monitor
this is to allow me to test the server by making it easy to select 
you're gonna need to change the window definition line of `basic_custom.cpp` to make it do what its final intended behavior is going to be.

# building
open x64 native tools command prompt for VS 2019 (requires at least installing build tools for VS 2019)
navigate to this folder  
type: `nmake -f makefile`  
look in the `bin` folder  
`basic_custom.exe`  
voilà  
only works in windows tho :(  
indeed the server and client to test it both require windows-specific libraries
so yeah... no linux/mac support anytime in the near future (if ever)