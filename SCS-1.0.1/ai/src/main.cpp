#include "GL/glut.h"
#include "scs.h"
#include <fstream>

#include "sim.h"
#pragma comment(lib, "smicar.lib")

using namespace std;

unsigned char graph[GRAPH_HEIGHT][GRAPH_WIDTH];

void Display ()
{
	glTranslated (-0.5, -0.5, 0.0);
	glScaled (0.2 ,0.2 ,0.2);
	glColor3d (1.0 ,0.0 ,0.0);
	
	glBegin (GL_LINE_STRIP);
	for (int i=0; i<5; i++) {
		glVertex2d ((double)i ,1.0);
		glVertex2d ((double)i ,0.0);
	}
	glEnd ();
	
	glLoadIdentity ();
	glTranslated (0.0, 0.5, 0.0);
	glColor3d (0.0, 1.0, 0.0);
	glRectd (-0.1, -0.1, 0.1, 0.1);
}


void AI_Camera ()
{
	int i=GRAPH_HEIGHT/2,j;
	int left,right,middle,dir,voltage,Threshold = 100;
	double speed;

	sGetGraph (graph);

	for (j=GRAPH_WIDTH/2; j>0; j--)
		if (graph[i][j]<Threshold) break;
	left = j;
	for (j=GRAPH_WIDTH/2; j<GRAPH_WIDTH-1; j++)
		if (graph[i][j]<Threshold) break;
	right = j;
	middle = (left+right)/2;

	dir = (middle-GRAPH_WIDTH/2) * 300 / GRAPH_WIDTH;
	sSetServoDir (dir);
	
	speed = sGetSpeed();
	voltage = (int)((1.5-speed)*10.0 + 5.0);
	sSetMotor (voltage);
}



void writeFile(double left, double right, double speed)
{
	ofstream ofile("input.txt");
	ofile << left << " " << right << " " << speed << endl;
	ofile.close();
}

SIMCAR_Interpreter     *interpreter;
void AI_Electromagnetic ()
{
	int dir ,voltage;
	double speed;
	double left ,right;

	sVector pos (-0.1, 0.25, 0.05);
	left = sGetMagnetic (pos).GetX ();

	pos.set (0.1, 0.25, 0.05);
	right = sGetMagnetic (pos).GetX ();
	/*
	dir = (int)(right - left) * 5;
	sSetServoDir (dir);

	speed = sGetSpeed ();
	voltage = (int )((1.0 - speed )*10.0 + 5.0);
	sSetMotor (voltage );
	*/
	speed = sGetSpeed ();
	writeFile(left, right, speed);

	SIMCAR_interpret(interpreter);

	ifstream ifile1("output1.txt");
	ifstream ifile2("output2.txt");

	ifile1 >> dir;
	ifile2 >> voltage;
	sSetServoDir (dir);
	sSetMotor (voltage );
}


unsigned char line[GRAPH_WIDTH];

void AI_Balance ()
{
	static double Angle = 0.0;
	static double s = 0.0;
	int i,left,right,middle,dir,Threshold = 100;
	double voltage,speed;
	sVector AngularSpeed = sGetAngularSpeed ();
	Angle += AngularSpeed.GetX ();

	speed = sGetSpeed ();
	s += speed;
	s -= 0.3;
	voltage = Angle*10.0 + AngularSpeed.GetX ()*10.0 + s*-20.0 + speed*-10.0;

	sGetLine (line);
	for (i=GRAPH_WIDTH/2; i>0; i--)
		if (line[i]<Threshold) break;
	left = i;

	for (i=GRAPH_WIDTH/2; i<GRAPH_WIDTH-1; i++)
		if (line[i]<Threshold) break;
	right = i;
	middle = (left+right)/2;
	
	dir = (middle-GRAPH_WIDTH/2) / 10;

	sSetMotorL (+dir-(int)voltage);
	sSetMotorR (-dir-(int)voltage);
}



//SIMCAR_Interpreter     *interpreter;
FILE *fp;
int main(int argc, char *argv[])
{

    if (argc != 2) {
        fprintf(stderr, "usage:%s filename", argv[0]);
        exit(1);
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "%s not found.\n", argv[1]);
        exit(1);
    }
	interpreter = SIMCAR_create_interpreter();
    SIMCAR_compile(interpreter, fp);


	switch (2) {
		default:
		case 0:
			sSetCar (camera);
			sSetAiFunc (AI_Camera);
			sEnableCustomWindow ();
			sSetDisplayFunc (Display);
			break;
		case 1:
			sSetCar (balance);
			sSetAiFunc (AI_Balance);
			sSetDepressionAngle (60.0);
			break;
		case 2:
			sSetCar (electromagnetic);
			sSetAiFunc (AI_Electromagnetic);
			break;
	}

	
	sSetTrack ("track/final8.trk");
    sRegister("I've read the license. And I accept it.");
	scsMainLoop (&argc,argv);

	return 0;
}
