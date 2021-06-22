/*  Copyright © 2018, Roboti LLC

    This file is licensed under the MuJoCo Resource License (the "License").
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.roboti.us/resourcelicense.txt
*/


#include "mujoco.h"
#include "glfw3.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// for UDP connectivity and threading thereof
#include<stdio.h>
#include<winsock2.h>
#include <thread>

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define BUFLEN 512  //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data

/*
// for debugging
#include <iostream>
using namespace std;
*/

// ------GLOBAL-------
// MuJoCo data structures
mjModel* m = NULL;                  // MuJoCo model, * in declaration means "pointer is defined" (whereas, outside a declaration, it means "value pointed to")
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

// mouse interaction
bool button_left = false;
bool button_middle = false;
bool button_right =  false;
double lastx = 0;
double lasty = 0;


/*
// convert bool to char (debug)
inline const char * const BoolToString(bool b)
{
  return b ? "true" : "false";
}
*/

// for the UDP socket
SOCKET s;
struct sockaddr_in server, si_other;
int slen , recv_len;
char buf[BUFLEN];
WSADATA wsa;


// ------- METHODS ----------
// keyboard callback
void keyboard(GLFWwindow* window, int key, int scancode, int act, int mods)
{
    // backspace: reset simulation
    if( act==GLFW_PRESS && key==GLFW_KEY_BACKSPACE )
    {
        mj_resetData(m, d);
        mj_forward(m, d);
		mju_copy3(CUMPERT,ZEROPERT);
		mju_copy3(pert.refpos,ZEROPERT);
		/*
		cout << "bksp" << endl;
		*/
    }
	
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


// socket initialize
void sockinit(void)
{
    // Define slen
    slen = sizeof(si_other) ;

    // Initialise winsock
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialised.\n");

    // Create a socket
    if((s = socket(AF_INET , SOCK_DGRAM , 0 )) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d" , WSAGetLastError());
    }
    printf("Socket created.\n");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( PORT );

    //Bind
    if( bind(s ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR)
    {
        printf("Bind failed with error code : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    puts("Bind done");
	
	fflush(stdout);
    memset(buf,'\0', BUFLEN); //clear the buffer by filling null, it might have previously received 
}

// socket thread (okay, mayyyybe worry about multithreading later. for now, just call a "readfrom" command for every iteration of the model. like a real dumb guy.)
// METHOD GOES HERE (eventually)

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

    // initialize UDP server
    sockinit();

    // make data
    d = mj_makeData(m);

    // init GLFW
    if( !glfwInit() )
        mju_error("Could not initialize GLFW");

    // create window, make OpenGL context current, request v-sync
    GLFWwindow* window = glfwCreateWindow(1920, 600, "Demo", NULL, NULL); // 3840, 1200 (two monitors' span, in other words) (cross-check against ACTUAL size of your monitors of course!)
	glfwSetWindowPos(window,0,0);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // initialize visualization data structures
    mjv_defaultCamera(&cam);
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);

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
		
		// apply perturbations, then update model
		// reset perturbation state
		pert.active = 1; // 1=translation, 2=rotation
		pert.select = 1;
		pert.skinselect = -1;
		mjv_applyPerturbPose(m, d, &pert, 0);  // move mocap bodies only

        // update scene and render
        mjv_updateScene(m, d, &opt, &pert, &cam, mjCAT_ALL, &scn);
        mjr_render(viewport, &scn, &con);

        // swap OpenGL buffers (blocking call due to v-sync)
        glfwSwapBuffers(window);

        // process pending GUI events, call GLFW callbacks
        glfwPollEvents();

        // -------------------
        // process UDP data, make this the last step so that the next iteration of the model is as up-to-date as possible
		// find a way to not make it HANG the simulation, but rather merely poll for new data
		// ...which will probably require multithreading, since the function you use to poll the data (recvfrom) holds up your script until it receives data (so you only want it to hang up your listener thread, instead of the entire main one)
		// ughhhhh but multithreading seems finnikcy! i don't *~wanna~* waaaaa waaaaa
        fflush(stdout); // commenting this & the following line out doesn't seem to do anything... provided I also include them in the init method (i.e., this probably does not have to happen on every iteration per se, maybe it adds robustness in cases that I cannot anticipate, though)
        memset(buf,'\0', BUFLEN); //clear the buffer by filling null, it might have previously received data

        //try to receive some data, this is a blocking call
        //(I suspect that this will hang the simulation until it receives info)
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
        {
            printf("recvfrom() failed with error code : %d" , WSAGetLastError());
        }

        //print details of the client/peer and the data received
        printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        printf("Data: %s\n" , buf);

        //now reply the client with the same data
        if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == SOCKET_ERROR)
        {
            printf("sendto() failed with error code : %d" , WSAGetLastError());
        }
        // -------------------
    }

    // close socket & do cleanup
    closesocket(s);
    WSACleanup();

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
