/*  Copyright © 2018, Roboti LLC

    This file is licensed under the MuJoCo Resource License (the "License").
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.roboti.us/resourcelicense.txt
*/

// include some things for use with the socket programming aspects... do this BEFORE the mujoco stuff, since you don't use include guards for these and the MuJoCo includes overwrite these (I believe glfw gets overwritten by winsock2... not totally sure tho)
#include <winsock2.h>
#include <thread>
#include <chrono>
#include <mutex>

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define BUFLEN 512  //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data

// and now your regularly scheduled mjc includes (with a couple extra at the end)
#include "mujoco.h" //includes mjui.h
#include "glfw3.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <windows.h>
#include <iostream>



// MuJoCo data structures
mjModel* m = NULL;                  // MuJoCo model
mjData* d = NULL;                   // MuJoCo data
mjvCamera cam;                      // abstract camera
mjvOption opt;                      // visualization options
mjvScene scn;                       // abstract scene
mjrContext con;                     // custom GPU context
mjvPerturb pert;                    // perturbation, need this to move shit around
/*
mjuiState uistate;                  // uistate, needed to process some UI events
*/

// set the perturbation values
const mjtNum perturbon [3] = {0.1,0,0};
mjtNum perturbcum [3] = {0,0,0};


// mouse interaction
bool button_left = false;
bool button_middle = false;
bool button_right =  false;
double lastx = 0;
double lasty = 0;

// server globals
SOCKET s;
struct sockaddr_in server, si_other;
int slen , recv_len;
char buf[BUFLEN];
WSADATA wsa;
char gbuf[BUFLEN];

// thread synchronization (to avoid simultaneous read/write requests)
std::mutex mtx;

// keyboard callback
void keyboard(GLFWwindow* window, int key, int scancode, int act, int mods)
{
    // backspace: reset simulation
    if( act==GLFW_PRESS && key==GLFW_KEY_BACKSPACE )
    {
        mj_resetData(m, d);
        mj_forward(m, d);
        std::cout << "wowzers\n";
    }

    /*
    // THIS WAS ONLY A TEST
    // spacebar press: apply perturbation
    if( act=GLFW_PRESS && key==GLFW_KEY_SPACE )
    {
        // mju_add3(pert.refpos,d->qpos,perturbon);
        mju_add3(perturbcum,perturbcum,perturbon);
        mju_copy3(pert.refpos,perturbcum);
        std::cout << "Hello\n";
    }

    // spacebar release: release perturbation
    if ( act=GLFW_PRESS && key==GLFW_KEY_A )
    {
        // mju_add3(pert.refpos,d->qpos,perturboff);
        mju_sub3(perturbcum,perturbcum,perturbon);
        mju_copy3(pert.refpos,perturbcum);
        std::cout << "Goodbye\n";
    }
    */
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


// server commands:
// init for server
void initserver(void)
{
    slen = sizeof(si_other) ;

    //Initialise winsock
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialised.\n");

    //Create a socket
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

    // initialize an empty buffer (probably not necessary)
    memset(buf,'\0', BUFLEN);
    strcpy (gbuf,buf); // assign not allowed because it's an array; note too that while we're writing to gbuf here, this isn't going to be multithreaded, so mutex control isn't required.
}

// server thread function
void serverfun(GLFWwindow* window)
{
    //keep listening for data
    while(!glfwWindowShouldClose(window))
    {
        // printf("Waiting for data...");
        fflush(stdout);

        //clear the buffer by filling null, it might have previously received data
        memset(buf,'\0', BUFLEN);

        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
        {
            printf("recvfrom() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }

        //now reply the client with the same data
        if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == SOCKET_ERROR)
        {
            printf("sendto() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }

        // assign to the g(lobal)buf
        mtx.lock(); // prevent simultaneous read/write when multithreading
        strcpy (gbuf,buf); // assign not allowed because it's an array
        mtx.unlock(); // resume normally scheduled programming
    }

    // cleanup
    closesocket(s);
    WSACleanup();
}


// report thread function
void reportfun(GLFWwindow* window)
{
    while(!glfwWindowShouldClose(window))
    {
        // wait
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        // report
        mtx.lock(); // prevent simultaneous read/write when multithreading (do I need to lock BOTH the read and the write of this shared resource? or just the write? just to be safe, I'll do both... for now... until it seems likely that it's limiting performance, anyway.)
        std::cout << "Current value is: \"" << gbuf << "\"" << std::endl;
        mtx.unlock(); // resume normally scheduled programming
    }
}


// main function
int main(int argc, const char** argv)
{
    // check command-line arguments (or not)
    /*
    if( argc!=2 )
    {
        printf(" USAGE:  basic modelfile\n");
        return 0;
    }
    */

    // activate software
    mj_activate("mjkey.txt");

    // load and compile model
    /*
    char error[1000] = "Could not load binary model";
    if( strlen(argv[1])>4 && !strcmp(argv[1]+strlen(argv[1])-4, ".mjb") )
        m = mj_loadModel(argv[1], 0);
    else
        m = mj_loadXML(argv[1], 0, error, 1000);
    if( !m )
        mju_error_s("Load model error: %s", error);
    */
    
    // if u wanna hard-code the loaded file... so that you can also hard-code model parameters to adjust
    const char* fileName = "..\\model\\scene.xml";
    char error[1000] = "Could not load model";
    m = mj_loadXML(fileName,0,error,1000);

    // init server
    initserver();


    // make data
    d = mj_makeData(m);

    // perturbations
    // set perturbation state
    pert.active = 1; // translation. 2 = rotation
    pert.select = 1; // body = 1. it's the only one in scene.xml
    pert.skinselect = -1; // not sure this really does anything, so just set to nothing

    // init GLFW
    if( !glfwInit() )
        mju_error("Could not initialize GLFW");

    // create window, make OpenGL context current, request v-sync
    // GLFWwindow* window = glfwCreateWindow(3840, 1080, "Demo", NULL, NULL); //width and height defined as first two elements here
    GLFWwindow* window = glfwCreateWindow(1920,540,"Demo",NULL,NULL);
    glfwSetWindowPos(window,0,0); //set position to top left corner of display
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

    // ======================= BEG NEWCODE =======================

    // stereo mode
    scn.stereo = mjSTEREO_SIDEBYSIDE;
    // ======================= END NEWCODE =======================

    // install GLFW mouse and keyboard callbacks
    glfwSetKeyCallback(window, keyboard);
    glfwSetCursorPosCallback(window, mouse_move);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetScrollCallback(window, scroll);

    // thread the server stuff
    std::thread serverthread(serverfun,window);
    std::thread reportthread(reportfun,window);

    // run main loop, target real-time simulation and 60 fps rendering
    while( !glfwWindowShouldClose(window) )
    {
        // advance interactive simulation for 1/60 sec
        //  Assuming MuJoCo can simulate faster than real-time, which it usually can,
        //  this loop will finish on time for the next frame to be rendered at 60 fps.
        //  Otherwise add a cpu timer and exit this loop when it is time to render.
        mjtNum simstart = d->time;
        while( d->time - simstart < 1.0/60.0 )
        {
            mj_step(m, d);
        }

        // get framebuffer viewport
        mjrRect viewport = {0, 0, 0, 0};
        glfwGetFramebufferSize(window, &viewport.width, &viewport.height);

        // update scene and render
        mjv_updateScene(m, d, &opt, NULL, &cam, mjCAT_ALL, &scn);
        mjr_render(viewport, &scn, &con);

        // swap OpenGL buffers (blocking call due to v-sync)
        glfwSwapBuffers(window);

        // process pending GUI events, call GLFW callbacks
        glfwPollEvents();

        // **********************************************************
        // add code where we:
        // set a perturbation to its qpos (qvel???) if the right arrow key is pressed
        // apply the perturbation
        // otherwise reset the perturbation

        // apply perturbations
        mjv_applyPerturbPose(m, d, &pert, 0);  // move mocap bodies only
        // **********************************************************
    }

    // close threads
    serverthread.join();
    reportthread.join();

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
