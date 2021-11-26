COMMON=/O2 /MT /EHsc /arch:AVX /I../include /Fe../bin-sample/

all:
	cl $(COMMON) testxml.cc                 ../bin-sample/mujoco210nogl.lib
	cl $(COMMON) testspeed.cc               ../bin-sample/mujoco210nogl.lib
	cl $(COMMON) compile.cc                 ../bin-sample/mujoco210nogl.lib
	cl $(COMMON) derivative.cc /openmp      ../bin-sample/mujoco210nogl.lib
	cl $(COMMON) basic.cc ../bin-sample/glfw3.lib  ../bin-sample/mujoco210.lib
	cl $(COMMON) record.cc ../bin-sample/glfw3.lib ../bin-sample/mujoco210.lib
	cl $(COMMON) simulate.cc ../include/uitools.c ../bin-sample/glfw3.lib ../bin-sample/mujoco210.lib
	del *.obj
