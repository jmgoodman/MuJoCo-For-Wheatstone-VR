#define MID_PRODUCER 10
#define MID_CONSUMER 11

#define MT_REQUEST_PERTURB_COMMAND 101
#define MT_PERTURB_COMMAND 102

typedef struct 
{ 
    double x; // it sure would be nice to just define this as a vector, but since the sample works with a struct (and it's the only one you know for sure how to convert between cpp and matlab), we gonna define the cartesian coordinates as a struct
    double y;
    double z;

} PERTURB_COMMAND;
