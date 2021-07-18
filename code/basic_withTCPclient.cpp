/*  Copyright © 2018, Roboti LLC

    This file is licensed under the MuJoCo Resource License (the "License").
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.roboti.us/resourcelicense.txt
*/

// mujoco includes
#include "mujoco.h"
#include "glfw3.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

/*
// math includes
#include <math.h>       // atan2
*/

// tcp includes
#include<winsock2.h> // for sockets
#include<iostream> // for printing
#include<chrono> // for sleep
#include<thread> // for sleep
#include<windows.h> // for opening wavefront
// #include<mutex> // to avoid simultaneous read-write problems

#pragma comment(lib,"ws2_32.lib") //Winsock Library

// tcp defines
#define SERVER "127.0.0.1"
#define PORT 3030
#define SAMPLEPERIOD 16

// it works! it tracks!
// todo:
// remove debugging text output
// find opengl-native way to flip rendering - obs is tooooo slow!!! (at least on my work machine - i should test on my home machine to see if it's a problem there, and if not, request an upgrade)

/*
// for debugging
#include <iostream>
using namespace std;
*/

// MuJoCo data structures
mjModel* m = NULL;                  // MuJoCo model
mjData* d = NULL;                   // MuJoCo data
mjvCamera cam;                      // abstract camera
mjvOption opt;                      // visualization options
mjvScene scn;                       // abstract scene
mjrContext con;                     // custom GPU context

// initialize perturbation
mjvPerturb pert;

mjtNum CUMPERT [3] = {0,0,0};
const mjtNum VERTPERT [3] = {0,0.167,0};
const mjtNum HORZPERT [3] = {0.167,0,0};
const mjtNum OUTPERT  [3] = {0,0,0.167}; 
const mjtNum ZEROPERT [3] = {0,0,0};
mjtNum ZEROLOC [3] = {0,0,0};

// mouse interaction
bool button_left = false;
bool button_middle = false;
bool button_right =  false;
double lastx = 0;
double lasty = 0;

// tcp globals
WSADATA wsa;
SOCKET s;
struct sockaddr_in server;

// thread locking (or not, it's probably just a source of bugs tbf)
// std::mutex mtx;


/*
// convert bool to char (debug)
inline const char * const BoolToString(bool b)
{
  return b ? "true" : "false";
}
*/


// keyboard callback
void keyboard(GLFWwindow* window, int key, int scancode, int act, int mods)
{
    // backspace: reset simulation
    if( act==GLFW_PRESS && key==GLFW_KEY_BACKSPACE )
    {
        mj_resetData(m, d);
        mj_forward(m, d);
		mju_copy3(CUMPERT,ZEROPERT);
		mju_copy3(pert.refpos,ZEROPERT); // refquat can also be used to adjust orientation - real x y z order
		/*
		cout << "bksp" << endl;
		*/
    }
	
	// keyboard buttons: space sets the lookat point
	// (at least this one functions normally...)
	if( act==GLFW_PRESS && key==GLFW_KEY_SPACE )
    {
		mju_copy3(cam.lookat,ZEROLOC);
    }
	
	// keyboard buttons: enter sets the camera position w.r.t. the lookat point
	// although calibrating to eye location is probably not possible, as monkey face is outside the tracking region
	// might be better to calibrate handrest location and hard-code the monkey eye position w.r.t. those two objects (which we can do with head fixation!!!)
	/*
	if( act==GLFW_PRESS && key==GLFW_KEY_ENTER )
    {
		// okay something screwy is going on here. multiple presses of enter, even from the same point, result in wildly different cameras...
		// so, for now, I'm disabling this. at least until I can figure out what the hell is going on here.
		mjtNum dx [3] = {0,0,0};
		mju_sub3(dx,ZEROLOC,cam.lookat);
		mjtNum dist = mju_dist3(ZEROLOC,cam.lookat); // dist parameter gotten here
		mjtNum azi  = mju_atan2(-dx[1],-dx[0]); // azimuth parameter
		mjtNum elv  = mju_asin(-dx[2] / dist); // elevation parameter
		cam.distance  = dist;
		cam.azimuth   = azi * 180/mjPI; // needs degrees!
		cam.elevation = elv * 180/mjPI;
		mju_copy3(cam.lookat,ZEROLOC);
    }
	*/
	
	// keyboard buttons: apply perturbations (only suitable when a single_body model, e.g. scene.xml, is loaded)
	if( act==GLFW_PRESS && key==GLFW_KEY_UP )
    {
		mju_add3(CUMPERT,CUMPERT,VERTPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "up" << endl;
		*/
    }
	
	if( act==GLFW_PRESS && key==GLFW_KEY_RIGHT )
    {
		mju_add3(CUMPERT,CUMPERT,HORZPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "right" << endl;
		*/
    }
	
	if( act==GLFW_PRESS && key==GLFW_KEY_DOWN )
    {
		mju_sub3(CUMPERT,CUMPERT,VERTPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "down" << endl;
		*/
    }
	
	if( act==GLFW_PRESS && key==GLFW_KEY_LEFT )
    {
		mju_sub3(CUMPERT,CUMPERT,HORZPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "left" << endl;
		*/
    }
	
	if( act==GLFW_PRESS && key==GLFW_KEY_A )
    {
		mju_add3(CUMPERT,CUMPERT,OUTPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "a" << endl;
		*/
    }
	
	if( act==GLFW_PRESS && key==GLFW_KEY_S )
    {
		mju_sub3(CUMPERT,CUMPERT,OUTPERT);
		mju_copy3(pert.refpos,CUMPERT);
		/*
		cout << "s" << endl;
		*/
    }
}


// mouse button callback
void mouse_button(GLFWwindow* window, int button, int act, int mods)
{
    // update button state
    button_left =   (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS);
    button_middle = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)==GLFW_PRESS);
    button_right =  (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS);

    // update mouse position
    glfwGetCursorPos(window, &lastx, &lasty);
	
	/*
	// spit out test number
	cout << BoolToString(scn.enabletransform) << endl;
	*/
}


// mouse move callback
void mouse_move(GLFWwindow* window, double xpos, double ypos)
{
    // no buttons down: nothing to do
    if( !button_left && !button_middle && !button_right )
        return;

    // compute mouse displacement, save
    double dx = xpos - lastx;
    double dy = ypos - lasty;
    lastx = xpos;
    lasty = ypos;

    // get current window size
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // get shift key state
    bool mod_shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);

    // determine action based on mouse button
    mjtMouse action;
    if( button_right )
        action = mod_shift ? mjMOUSE_MOVE_H : mjMOUSE_MOVE_V;
    else if( button_left )
        action = mod_shift ? mjMOUSE_ROTATE_H : mjMOUSE_ROTATE_V;
    else
        action = mjMOUSE_ZOOM;

    // move camera
    mjv_moveCamera(m, action, dx/height, dy/height, &scn, &cam);
}


// scroll callback
void scroll(GLFWwindow* window, double xoffset, double yoffset)
{
    // emulate vertical mouse motion = 5% of window height
    mjv_moveCamera(m, mjMOUSE_ZOOM, 0, -0.05*yoffset, &scn, &cam);
}

// tcp client init
int tcpinit(void)
{
	// now get to the actual server-client stuff
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed. Error Code : %d",WSAGetLastError());
		return 1;
	}
	
	printf("Initialised.\n");
	
	//Create a socket
	if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d" , WSAGetLastError());
		return 1;
	}

	printf("Socket created.\n");
	
	
	server.sin_addr.s_addr = inet_addr(SERVER); // localhost
	server.sin_family = AF_INET;
	server.sin_port = htons( PORT );

	//Connect to remote server
	if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		puts("connect error");
		return 1;
	}
	
	puts("Connected");
	return 0;
}

// tcp parse and send command
int sendcommand(const char *thismsg)
{	
	// ----------------------------
	int msglen = strlen(thismsg); // do NOT include nullterm, it causes problems when subsequently trying to read out the dataframes. namely, if the server receives a null terminated command, it simply does not send the dataframes (or it does, BUT its "OK" confirmation message includes the null terminating character which in turn completely fucks with the parsing of the confirmation method and, in turn, the subsequent dataframe)
	char lenbytes[4];
	char typebytes[4];
	int b = 0;

	memset(lenbytes,'\0',4);
	memset(typebytes,'\0',4);

	lenbytes[0]  = ((msglen+8) >> 24) & (int)(0xFF); // cast the hex values (1 byte, i.e. chars) as ints, to avoid relying on "implicit" zeros when doing a bitwise comparison
	lenbytes[1]  = ((msglen+8) >> 16) & (int)(0xFF); 
	lenbytes[2]  = ((msglen+8) >> 8)  & (int)(0xFF);
	lenbytes[3]  = (msglen+8) & (int)(0xFF);
	typebytes[3] = 1; // commands are always just 1, no need to do long calculations

	// std::cout << "len: " << (msglen+8) << std::endl;
	
	// build your message
	char * message = new char [msglen+8]; //make the message length EXACTLY as long as it needs to be. don't try to buffer it, that'll lead to out-of-bounds error messages in the Wavefront console (and may have contributed to the lack of streamed data???)
	memset(message,'\0',msglen+8); // initialize buffer like so
	
	while(b<4)
	{
		message[b] = lenbytes[b];
		b++;
	}
	
	while(b<8)
	{
		message[b] = typebytes[b-4];
		b++;
	}
	
	while(b<(msglen+8)) // exclude the last bit, keep it null to terminate your command string
	{
		message[b] = thismsg[b-8];
		b++;
	}
	
	// now send
	if( send(s , message , (msglen+8) , 0) < 0)
	{
		puts("Send failed");
		return 1;
	}
	// puts("Data Send\n"); // needlessly clutters your output stream
	return 0;
}

// read confirmation
int readconfirm(void)
{
	//Receive a reply from the server
	//start with the size parameter
	char size_reply[4];
	int recv_size;
	if((recv_size=recv(s , size_reply , 4 , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	// convert to int
	int size_reply_int = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		size_reply_int <<= 8;
		size_reply_int += (int)size_reply[i];
	}
	
	size_reply_int = size_reply_int - 8;
	
	// std::cout << "size = \"" << size_reply_int << "\"" << std::endl << std::endl;
	
	// next get the type parameter
	char type_reply[4];
	if((recv_size=recv(s , type_reply , 4 , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	// convert to int
	int type_reply_int = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		type_reply_int <<= 8;
		type_reply_int += (int)type_reply[i];
	}
	
	// std::cout << "type = \"" << type_reply_int << "\"" << std::endl << std::endl;
	
	// next get the message per se
	char *mssg_reply;
	mssg_reply = new char [size_reply_int]; // because we already read 8 bits of it
	if((recv_size=recv(s , mssg_reply , size_reply_int , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	char *mssg_toprint;
	mssg_toprint = new char [size_reply_int + 1];
	memset(mssg_toprint,'\0',size_reply_int+1);
	memcpy(mssg_toprint,mssg_reply,size_reply_int);

	// std::cout << "message = \"" << mssg_toprint << "\"" << std::endl << std::endl;
	return 0;
}

// read dataframe if one is available
int readdataframe(void)
{
	// now read in the dataframe
	//start with the size parameter
	char size_reply[4];
	int recv_size;
	if((recv_size=recv(s , size_reply , 4 , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	// convert to int
	int size_reply_int = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		size_reply_int <<= 8;
		size_reply_int += (int)size_reply[i];
	}
	
	size_reply_int = size_reply_int - 8;
	// std::cout << "size dataframe = \"" << size_reply_int << "\"" << std::endl << std::endl;
	
	// next get the type parameter
	char type_reply[4];
	if((recv_size=recv(s , type_reply , 4 , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	// convert to int
	int type_reply_int = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		type_reply_int <<= 8;
		type_reply_int += (int)type_reply[i];
	}
	
	// std::cout << "type dataframe= \"" << type_reply_int << "\"" << std::endl << std::endl;
	
	// next get the message per se
	char *mssg_reply;
	mssg_reply = new char [size_reply_int]; // because we already read 8 bits of it
	if((recv_size=recv(s , mssg_reply , size_reply_int , 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
		return 1;
	}
	
	// std::cout << "message dataframe:" << std::endl;
	
	// read out the message bytes
	int ii = 0;

	while(ii<(size_reply_int))
	{
		// std::cout << ii << "|" << (unsigned int)mssg_reply[ii] << " ";
		// std::cout << std::hex << (unsigned int)mssg_reply[ii];
		
		// uncomment the line below to get the raw bytestring output
		// printf("%.2X",(unsigned char)mssg_reply[ii]);
		if ( ii % 2 == 1 )
		{
			// std::cout << " ";
		}
		ii++;
	}
	
	// std::cout << std::endl;
	
	// parse the fields
	
	// componentcount
	unsigned int component_count = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		component_count <<= 8;
		component_count += (unsigned int)mssg_reply[i];
	}
	
	// std::cout << "Component Count: " << component_count << std::endl;
	
	// componentsize
	unsigned int componentsize = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		componentsize <<= 8;
		componentsize += (unsigned int)mssg_reply[i+4];
	}
	
	// std::cout << "Component Size: " << componentsize << std::endl;

	// componenttype
	unsigned int componenttype = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		componenttype <<= 8;
		componenttype += (unsigned int)mssg_reply[i+8];
	}
	
	// std::cout << "Component Type: " << componenttype << std::endl;

	// framenumber
	unsigned int framenumber = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		framenumber <<= 8;
		framenumber += (unsigned int)mssg_reply[i+12];
	}
	
	// std::cout << "Frame Number: " << framenumber << std::endl;

	// timestamp
	unsigned long long timestamp = 0;
	for (int i = 0; i <= 7; i++) // big endian
	{
		timestamp <<= 8;
		timestamp += (unsigned long long)mssg_reply[i+16];
	}
	
	// std::cout << "Timestamp (ms): " << timestamp << std::endl;
	
	// toolcount
	unsigned int toolcount = 0;
	for (int i = 0; i <= 3; i++) // big endian
	{
		toolcount <<= 8;
		toolcount += (unsigned int)mssg_reply[i+24];
	}
	
	// std::cout << "Tool Count: " << toolcount << std::endl;
	

	unsigned int tooliter = 0;
	unsigned int byteind  = 28;
	
	while (tooliter < toolcount)
	{
		// Q0
		float Q0;
		unsigned char Q0_[4];
		for (int i = 0; i <= 3; i++)
		{
			Q0_[i] = mssg_reply[3-i+byteind]; // dataframe is lil endian, everything else until now was big endian
		}
		
		memcpy(&Q0,Q0_,sizeof(Q0)); // don't need to pointerize Q0_ because, as an array, it is already a pointer
		// std::cout << "Tool " << tooliter+1 << " Q0: " << Q0 << std::endl;
		
		// Qx
		float Qx;
		unsigned char Qx_[4];
		for (int i = 0; i <= 3; i++)
		{
			Qx_[i] = mssg_reply[3-i+byteind+4];
		}
		
		memcpy(&Qx,Qx_,sizeof(Qx));
		// std::cout << "Tool " << tooliter+1 << " Qx: " << Qx << std::endl;
					
		// Qy
		float Qy;
		unsigned char Qy_[4];
		for (int i = 0; i <= 3; i++)
		{
			Qy_[i] = mssg_reply[3-i+byteind+8];
		}
		
		memcpy(&Qy,Qy_,sizeof(Qy));
		// std::cout << "Tool " << tooliter+1 << " Qy: " << Qy << std::endl;

		// Qz
		float Qz;
		unsigned char Qz_[4];
		for (int i = 0; i <= 3; i++)
		{
			Qz_[i] = mssg_reply[3-i+byteind+12];
		}
		
		memcpy(&Qz,Qz_,sizeof(Qz));
		// std::cout << "Tool " << tooliter+1 << " Qz: " << Qz << std::endl;

		// X
		float X;
		unsigned char X_[4];
		for (int i = 0; i <= 3; i++)
		{
			X_[i] = mssg_reply[3-i+byteind+16];
		}
		
		memcpy(&X,X_,sizeof(X));
		// std::cout << "Tool " << tooliter+1 << " X: " << X << std::endl;

		// Y
		float Y;
		unsigned char Y_[4];
		for (int i = 0; i <= 3; i++)
		{
			Y_[i] = mssg_reply[3-i+byteind+20];
		}
		
		memcpy(&Y,Y_,sizeof(Y));
		// std::cout << "Tool " << tooliter+1 << " Y: " << Y << std::endl;

		// Z
		float Z;
		unsigned char Z_[4];
		for (int i = 0; i <= 3; i++)
		{
			Z_[i] = mssg_reply[3-i+byteind+24];
		}
		
		memcpy(&Z,Z_,sizeof(Z));
		// std::cout << "Tool " << tooliter+1 << " Z: " << Z << std::endl;r
		
		// RMS
		float rms; // PROBABLY a float, the user guide doesn't actually specify, but RMS values ought to be floats... right??? unless this is a flag for EXCESSIVE error... in which case this will either give a perfect 0 OR a very very tiny float value as its output.
		unsigned char rms_[4];
		for (int i = 0; i <= 3; i++)
		{
			rms_[i] = mssg_reply[3-i+byteind+28];
		}
		
		memcpy(&rms,rms_,sizeof(rms));
		
		// only print if rms is not zero
		if ( rms > 0 )
		{
			std::cout << "Tool " << tooliter+1 << " RMS marker fit to rigid body error: " << rms << std::endl;
		}
		

		// if tool = 1, set perturbation. also, check the data for NaN values so that you can maintain previous position instead of flickering when you get a bad frame (eventually - TODO!!!)
		if(tooliter<7)
		{
			// provided none of these values are outside of +/- 500
			
			if ( (X > -500) && (X < 500) && (Y > -500) && (Y < 500) && (Z > -500) && (Z < 500) ) // only update if the numbers make sense. otherwise, just keep the previous state
			{
				mjtNum pospert [3] = {-Y,X,-Z}; // hopefully this converts floats to doubles without a hitch... also, axes are rearranged to transfer from wavefront to mujoco conventions (and also to "flip" the world for 3D presentation)
				mjtNum quatpert [4] = {Q0,Qy,-Qx,Qz}; // should be normalized when it comes outta wavefront... this also needed to go through a change to be appropriate for VR.
				
				// debug output
				/*
				system("CLS");
				std::cout << -Y << " " << X << " " << -Z << " " << Q0 << " " << Qx << " " << Qy << " " << Qz << " " << std::endl;
				*/
				
				// this all works with the wand, btw. when it comes to the hand with all 8 markers, it chugs like you wouldn't believe, and eventually, WaveFront fucking crashes. Need to solve this problem! (probably by splitting wavefront and mujoco onto different machines...) (ähm, okay, when I tried it again, it didn't cause problems anymore...)
				mjtNum scalefactor = 0.001; // convert mm (Wave) to m (MuJoCo)
				
				mju_scl3(pert.refpos,pospert,scalefactor); // was copy3, but i realized that I need to convert from mm to m.
				mju_copy4(pert.refquat,quatpert); // refquat can also be used to adjust orientation - real x y z order
				
				// also use the perturbation location to define your zero location
				mju_copy3(ZEROLOC,pert.refpos);
				
				// apply perturbation to the appropriate body
				pert.active = 1; // 1=translation, 2=rotation
				pert.select = tooliter+1;
				pert.skinselect = -1;
				mjv_applyPerturbPose(m, d, &pert, 0);  // move mocap bodies only
			}

		}
		
		byteind = byteind + 32;
		tooliter++;
	}
}

// cleanup
int tcpcleanup(void)
{
	closesocket(s);
	puts("socket closed\n");
	WSACleanup();
	puts("winsock unloaded");
	return 0;
}


// tcp client function for thread
int clientfun(GLFWwindow* window)
{
	if(tcpinit()==1)
	{
		puts("init failed");
		return 1;
	}
	
	if(sendcommand("Version 1.0")==1)
	{
		puts("send Version 1.0 failed");
		tcpcleanup();
		return 1;
	}
	
	if(readconfirm()==1)
	{
		puts("reading Version 1.0 confirmation failed");
		tcpcleanup();
		return 1;
	}
	
	// didn't work, pill didn't move :(
	/*
	if(sendcommand("StreamFrames AllFrames")==1)
	{
		puts("send StreamFrames AllFrames failed");
		tcpcleanup();
		return 1;
	}
	
	if(readconfirm()==1)
	{
		puts("reading StreamFrames AllFrames confirmation failed");
		tcpcleanup();
		return 1;
	}
	*/
	
	while(!glfwWindowShouldClose(window))
	{
		// std::this_thread::sleep_for(std::chrono::milliseconds(SAMPLEPERIOD));
		
		// this is probably mega inefficient. simply call "streamframes" once.
		// uh oh, problem: "StreamFrames AllFrames" doesn't result in movement! oh no! I have to assume the problem is that the format is juuuuust slightly different, which in turn fucks up my parsing. UGHHHHH
		
		if(sendcommand("SendCurrentFrame")==1)
		{
			puts("send SendCurrentFrame failed");
			tcpcleanup();
			return 1;
		}
		
		if(readconfirm()==1)
		{
			puts("reading SendCurrentFrame confirmation failed");
			tcpcleanup();
			return 1;
		}
		
		
		// mtx.lock(); // prevent reading while writing (OR NOT, I THINK THIS IS THE SOURCE OF YOUR SLOWDOWN AND YOUR BUGS!!!)
		if(readdataframe()==1)
		{
			puts("reading SendCurrentFrame dataframe failed");
			tcpcleanup();
			// mtx.unlock();
			return 1;
		}
		// mtx.unlock();
	}
	
	
	// only use with StreamFrames AllFrames
	/*
	sendcommand("StreamFrames Stop");
	readconfirm();
	*/
	
	tcpcleanup();
	return 0;
}

// main function
int main(int argc, const char** argv)
{
    // check command-line arguments
    if( argc!=2 )
    {
        printf(" USAGE:  basic modelfile\n");
        return 0;
    }

    // activate software
    mj_activate("mjkey.txt");

    // load and compile model
    char error[1000] = "Could not load binary model";
    if( strlen(argv[1])>4 && !strcmp(argv[1]+strlen(argv[1])-4, ".mjb") )
        m = mj_loadModel(argv[1], 0);
    else
        m = mj_loadXML(argv[1], 0, error, 1000);
    if( !m )
        mju_error_s("Load model error: %s", error);

    // make data
    d = mj_makeData(m);

    // init GLFW
    if( !glfwInit() )
        mju_error("Could not initialize GLFW");

    // create window, make OpenGL context current, request v-sync
    GLFWwindow* window = glfwCreateWindow(3840, 1200, "Demo", NULL, NULL);
	glfwSetWindowPos(window,0,0);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // initialize visualization data structures
    mjv_defaultCamera(&cam);
	
	// set REAL camera defaults
    cam.lookat[0] = 0;
    cam.lookat[1] = 0;
    cam.lookat[2] = 0;
    cam.distance  = 0.2; // if this points at the object, then it should be a 15 (height), 20(out), 25 (hyp) triangle of distance
	cam.azimuth   = 90; // deg
	cam.elevation = 37; // deg, 37 for a nice 3-4-5 triangle

    // set to fixed camera
    // cam.type = mjCAMERA_FIXED;
	
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);
	
	// edit opt so perturbations cannot be seen with red arrows and whatnot
	
	opt.flags[mjVIS_PERTFORCE] = false;
	opt.flags[mjVIS_PERTOBJ] = false;

    // create scene and context
    mjv_makeScene(m, &scn, 2000);
    mjr_makeContext(m, &con, mjFONTSCALE_150);

    // install GLFW mouse and keyboard callbacks
    glfwSetKeyCallback(window, keyboard);
    glfwSetCursorPosCallback(window, mouse_move);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetScrollCallback(window, scroll);
	
	// stereo mode
    scn.stereo = mjSTEREO_SIDEBYSIDE;
	
	// run TCP thread
	std::thread tcpthread(clientfun,window);

    // run main loop, target real-time simulation and 60 fps rendering
    while( !glfwWindowShouldClose(window) )
    {
        // advance interactive simulation for 1/60 sec
        //  Assuming MuJoCo can simulate faster than real-time, which it usually can,
        //  this loop will finish on time for the next frame to be rendered at 60 fps.
        //  Otherwise add a cpu timer and exit this loop when it is time to render.
        mjtNum simstart = d->time;
        while( d->time - simstart < 1.0/60.0 )
            mj_step(m, d);

        // get framebuffer viewport
        mjrRect viewport = {0, 0, 0, 0};
        glfwGetFramebufferSize(window, &viewport.width, &viewport.height);
		
		// do this within the data reading loop 
		/*
		// apply perturbations, then update model
		// reset perturbation state
		pert.active = 1; // 1=translation, 2=rotation
		pert.select = 1;
		pert.skinselect = -1;
		mjv_applyPerturbPose(m, d, &pert, 0);  // move mocap bodies only
		*/

        // update scene and render
        mjv_updateScene(m, d, &opt, &pert, &cam, mjCAT_ALL, &scn);
        mjr_render(viewport, &scn, &con);

        // swap OpenGL buffers (blocking call due to v-sync)
        glfwSwapBuffers(window);

        // process pending GUI events, call GLFW callbacks
        glfwPollEvents();
    }
	
	// join threads
    tcpthread.join();

    //free visualization storage
    mjv_freeScene(&scn);
    mjr_freeContext(&con);

    // free MuJoCo model and data, deactivate
    mj_deleteData(d);
    mj_deleteModel(m);
    mj_deactivate();

    // terminate GLFW (crashes with Linux NVidia drivers)
    #if defined(__APPLE__) || defined(_WIN32)
        glfwTerminate();
    #endif

    return 1;
}
