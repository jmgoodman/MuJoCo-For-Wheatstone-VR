# MuJoCo-For-Wheatstone-VR
 
includes a modification of basic.cpp which includes an asynchronous UDP server for periodically reporting the last value sent from the client. This allows a separate client (e.g., tracking) to send whatever data it pleases, at whatever rate it pleases, and have MuJoCo query that data when *it* sees fit.

currently, the test script reports with a sluggish period of 2s. This can naturally be reduced, this script is not a final product but rather a demonstration-of-concept.

look in the `/code/` folder for build instructions.
