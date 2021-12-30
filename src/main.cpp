#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <string.h>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <string>
#include <vector>			//Standard template library class
#include <GL/glew.h>
#include <GL/glut.h>
//glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/half_float.hpp>
#include "shaders.h"    
#include "shapes.h"    
#include "lights.h"    

#define STB_IMAGE_IMPLEMENTATION
#include "STB/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "STB/stb_image_write.h"

#pragma warning(disable : 4996)
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "glut32.lib")

using namespace std;

bool needRedisplay=false;
ShapesC* sphere;
ShapesC* terrain;

//shader program ID
GLuint shaderProgram;
GLfloat ftime=0.f;
glm::mat4 view=glm::mat4(1.0);
glm::mat4 proj=glm::perspective(80.0f,//fovy
				  		        1.0f,//aspect
						        0.01f,1000.f); //near, far
class ShaderParamsC
{
public:
	GLint modelParameter;		//modeling matrix
	GLint modelViewNParameter;  //modeliview for normals
	GLint viewParameter;		//viewing matrix
	GLint projParameter;		//projection matrix
	//material
	GLint kaParameter;			//ambient material
	GLint kdParameter;			//diffuse material
	GLint ksParameter;			//specular material
	GLint shParameter;			//shinenness material
} params;


LightC light;


//the main window size
GLint wWindow=800;
GLint hWindow=800;

float sh=1;

#define MAX_PARTICLES 1000

struct Particle {
	glm::vec3 loc;
	glm::vec3 vel;
	glm::vec3 force;
	glm::vec3 color;
	float mass;
};

int t_width;
int t_height;
int num_particles = 50;

unsigned char* terrainData; 
vector<vector<GLfloat>> heightmap;
unsigned int land;

float delta_t = 0.01;
float gravity = 9.82;
float viscosity = 0;
float friction = 0.1;
float wind = 0;
Particle particles[MAX_PARTICLES];

static int window;
static int menu_id;
static int wind_menu;
static int viscosity_menu;
static int gravity_menu;
static int friction_menu;
static int particles_menu;
static int value = 0;

float lookX = -135.0f;
glm::vec3 cameraPos = glm::vec3(30.f, 10.f, 30.f);
glm::vec3 cameraFront = glm::vec3(-1.f, 0.f, -1.f);

/*********************************
Some OpenGL-related functions
**********************************/

//called when a window is reshaped
void Reshape(int w, int h)
{
  glViewport(0,0,w, h);       
  glEnable(GL_DEPTH_TEST);
//remember the settings for the camera
  wWindow=w;
  hWindow=h;
}

void menu(int num) {
	if (num == 0) {
		glutDestroyWindow(window);
		exit(0);
	}
	else {
		value = num;
	}
	glutPostRedisplay();
}
void createMenu(void) {
	wind_menu = glutCreateMenu(menu);
	glutAddMenuEntry("Increase", 3);
	glutAddMenuEntry("Decrease", 4);

	viscosity_menu = glutCreateMenu(menu);
	glutAddMenuEntry("Increase", 5);
	glutAddMenuEntry("Decrease", 6);

	gravity_menu = glutCreateMenu(menu);
	glutAddMenuEntry("Increase", 7);
	glutAddMenuEntry("Decrease", 8);

	friction_menu = glutCreateMenu(menu);
	glutAddMenuEntry("Increase", 9);
	glutAddMenuEntry("Decrease", 10);

	particles_menu = glutCreateMenu(menu);
	glutAddMenuEntry("Add 50", 11);
	glutAddMenuEntry("Delete 50", 12);

	menu_id = glutCreateMenu(menu);
	glutAddSubMenu("Wind", wind_menu);
	glutAddSubMenu("Viscosity", viscosity_menu);
	glutAddSubMenu("Gravity", gravity_menu);
	glutAddSubMenu("Friction", friction_menu);
	glutAddSubMenu("Particles", particles_menu);
	glutAddMenuEntry("Zero gravity", 1);
	glutAddMenuEntry("Friction one", 2);
	glutAddMenuEntry("Reset", 13);
	glutAddMenuEntry("Exit", 0);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void updateLocation(int i) {
	//zero out the forces
	particles[i].force = glm::vec3(0, 0, 0);

	//add gravity
	particles[i].force.y -= particles[i].mass * gravity;

	//add viscosity
	particles[i].force -= particles[i].vel * viscosity;

	//add wind
	particles[i].force.x += wind;

	particles[i].loc += delta_t * particles[i].vel;
	particles[i].vel += delta_t * particles[i].force / particles[i].mass;
}
float determineMax(float a, float b, float c) {
	float max = a;
	if (b > max)
		max = b;
	if (c > max)
		max = c;
	return max;
}
void detectCollision(int i) {
	
		//Round location of particle down to the closest tenth 
		//(X.1, X.2, X.3, etc)
		//Multiply by 10 to get the index for the heightmap
		int locX = (int)(10 * particles[i].loc.x);
		int locZ = (int)(10 * particles[i].loc.z);

		if (locX < t_width - 1 && locZ < t_height - 1 && locX > 1 && locZ > 1) {
			//^^That'll give us a box to check with two triangles
			//Compare the location to see which triangle it hit
			glm::vec3 v1, v2, v3, norm;
			float y1, y2, y3;
			if (particles[i].loc.x < particles[i].loc.z) {
				//first triangle
				y1 = heightmap[locX][locZ];
				v1 = glm::vec3(locX * 0.1f, y1, locZ * 0.1f);
				y2 = heightmap[locX][locZ + 1];
				v2 = glm::vec3(locX * 0.1f, y2, (locZ + 1) * 0.1f);
				y3 = heightmap[locX + 1][locZ];
				v3 = glm::vec3((locX + 1) * 0.1f, y3, locZ * 0.1f);
				norm = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			}
			else {
				//second triangle
				y1 = heightmap[locX + 1][locZ + 1];
				v1 = glm::vec3((locX + 1) * 0.1f, y1, (locZ + 1) * 0.1f);
				y2 = heightmap[locX + 1][locZ];
				v2 = glm::vec3((locX + 1) * 0.1f, y2, locZ * 0.1f);
				y3 = heightmap[locX][locZ + 1];
				v3 = glm::vec3(locX * 0.1f, y3, (locZ + 1) * 0.1f);
				norm = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			}

			float max = determineMax(y1, y2, y3);

			if (particles[i].loc.y <= max) {

				//Find the normal of the triangle and calculate velocity
				glm::vec3 normalVel = glm::dot(particles[i].vel, norm)*norm;
				glm::vec3 tangVel = particles[i].vel - normalVel;

				particles[i].vel = (1-friction)*glm::vec3(tangVel.x - normalVel.x, tangVel.y - normalVel.y, tangVel.z - normalVel.z);

			}
		}
		updateLocation(i);
}

//the main rendering function
void RenderObjects()
{
	const int range=3;
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	glClearColor(0.f, 0.4f, 0.6f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(0,0,0);
	glPointSize(2);
	glLineWidth(1);
	//set the projection and view once for the scene
	glUniformMatrix4fv(params.projParameter,1,GL_FALSE,glm::value_ptr(proj));
	view=glm::lookAt(cameraPos,//eye
				     cameraPos + cameraFront,  //destination
				     glm::vec3(0,1,0)); //up

	glUniformMatrix4fv(params.viewParameter,1,GL_FALSE,glm::value_ptr(view));
//set the light
	static glm::vec4 pos;
	pos.x = 20 * sin(ftime / 12); pos.y = -10; pos.z = 20 * cos(ftime / 12); pos.w = 1;
	light.SetPos(pos);
	light.SetShaders();

	terrain->Render();

	//draw particles
#pragma omp parallel for
	for (int i = 0; i < num_particles; i++) {
		glLoadIdentity();
		glm::mat4 m = glm::translate(glm::mat4(1.0), glm::vec3(0, 0, 0));
		m = glm::translate(m, particles[i].loc);
		m = glm::scale(m, glm::vec3(0.1f, 0.1f, 0.1f));
		sphere->SetKd(particles[i].color);
		sphere->SetModel(m);
		detectCollision(i);

		//now the normals
		glm::mat3 modelViewN = glm::mat3(view * m);
		modelViewN = glm::transpose(glm::inverse(modelViewN));
		sphere->SetModelViewN(modelViewN);
		sphere->Render();
	}
}
	
void Idle(void)
{
  glClearColor(0.1,0.1,0.1,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  ftime+=0.05;
  glUseProgram(shaderProgram);
  RenderObjects();
  glutSwapBuffers();  
}

void addParticles() {
	for (int i = num_particles - 50; i < num_particles; i++) {
		particles[i].loc.y = 15;
		particles[i].loc.x = (rand() % 140 + 10) / 6.f;
		particles[i].loc.z = (rand() % 140 + 10) / 6.f;
		particles[i].mass = (rand() % 5 + 1) / 5.0f;
		particles[i].color.x = rand() % 256 / 255.f;
		particles[i].color.y = rand() % 256 / 255.f;
		particles[i].color.z = rand() % 256 / 255.f;
		particles[i].vel = glm::vec3(0, 0, 0);
		particles[i].force = glm::vec3(0, 0, 0);
	}
}

void conditions() {
	switch (value){
	case 1: gravity = 0; break;
	case 2: friction = 0; break;
	case 3: wind+=0.2; break;
	case 4: wind-=0.2; break;
	case 5: if (viscosity < 1) viscosity+=0.1; break;
	case 6: if (viscosity > 0) viscosity-=0.1; break;
	case 7: gravity += 2; break;
	case 8: gravity -= 2; break;
	case 9: friction += 0.05; break;
	case 10: friction -= 0.05;  break;
	case 11: num_particles += 50; addParticles(); break;
	case 12: num_particles -= 50; break;
	case 13: gravity = 9.82; friction = 0.1; viscosity = 0; wind = 0; break;
	};

	if (friction > 1)
		friction = 1;
	else if (friction < 0)
		friction = 0;

	if (viscosity > 1)
		viscosity = 1;
	else if (viscosity < 0)
		viscosity = 0;

	if (num_particles > 1000)
		num_particles = 1000;
	else if (num_particles < 0)
		num_particles = 0;
}
void Display(void)
{
	RenderObjects();
	conditions();
}

//keyboard callback
void Kbd(unsigned char a, int x, int y)
{
	switch(a)
	{
 	  case 27 : exit(0);break;
	  case 'r': 
	  case 'R': {sphere->SetKd(glm::vec3(1,0,0));break;}
	  case 'g': 
	  case 'G': {sphere->SetKd(glm::vec3(0,1,0));break;}
	  case 'b': 
	  case 'B': {sphere->SetKd(glm::vec3(0,0,1));break;}
	  case 's': {cameraPos.y-=1; break;}
	  case 'w': {cameraPos.y+=1; break;}
	  case 'W': {sphere->SetKd(glm::vec3(0.7,0.7,0.7));break;}
	  case '+': {sphere->SetSh(sh+=1);break;}
	  case '-': {sphere->SetSh(sh-=1);if (sh<1) sh=1;break;}
	}
	cout << "shineness="<<sh<<endl;
	glutPostRedisplay();
}


//called when a special key is released
void SpecKbdRelease(int a, int x, int y)
{
   	switch(a)
	{
 	  case GLUT_KEY_LEFT  : 
		  {
			  break;
		  }
	  case GLUT_KEY_RIGHT : 
		  {
			break;
		  }
 	  case GLUT_KEY_DOWN    : 
		  {
			break;
		  }
	  case GLUT_KEY_UP  :
		  {
			break;
		  }

	}
	//glutPostRedisplay();
}

//called when a special key is pressed
void SpecKbdPress(int a, int x, int y)
{
	glm::vec3 direction;
	switch(a)
	{
 	  case GLUT_KEY_LEFT:
	  {
		  lookX -= 1;
		  break;
	  }
	  case GLUT_KEY_RIGHT:
	  {
		  lookX += 1;
		  break;
	  }
	  case GLUT_KEY_DOWN:
	  {
		  cameraPos -= cameraFront;
		  break;
	  }
	  case GLUT_KEY_UP:
	  {
		  cameraPos += cameraFront;
		  break;
	  }
	}
	direction.x = cos(glm::radians(lookX));
	direction.z = sin(glm::radians(lookX));
	cameraFront = glm::normalize(direction);
	//glutPostRedisplay();
}


void Mouse(int button, int state, int x, int y)
{
	cout << "Location is " << "[" << x << "'" << y << "]" << endl;
}


void InitializeProgram(GLuint* program)
{
	std::vector<GLuint> shaderList;

	//load and compile shaders 	
	shaderList.push_back(CreateShader(GL_VERTEX_SHADER, LoadShader("shaders/phong.vert")));
	shaderList.push_back(CreateShader(GL_FRAGMENT_SHADER, LoadShader("shaders/phong.frag")));

	//create the shader program and attach the shaders to it
	*program = CreateProgram(shaderList);

	//delete shaders (they are on the GPU now)
	std::for_each(shaderList.begin(), shaderList.end(), glDeleteShader);

	params.modelParameter = glGetUniformLocation(*program, "model");
	params.modelViewNParameter = glGetUniformLocation(*program, "modelViewN");
	params.viewParameter = glGetUniformLocation(*program, "view");
	params.projParameter = glGetUniformLocation(*program, "proj");
	//now the material properties
	params.kaParameter = glGetUniformLocation(*program, "mat.ka");
	params.kdParameter = glGetUniformLocation(*program, "mat.kd");
	params.ksParameter = glGetUniformLocation(*program, "mat.ks");
	params.shParameter = glGetUniformLocation(*program, "mat.sh");
	//now the light properties
	light.SetLaToShader(glGetUniformLocation(*program, "light.la"));
	light.SetLdToShader(glGetUniformLocation(*program, "light.ld"));
	light.SetLsToShader(glGetUniformLocation(*program, "light.ls"));
	light.SetLposToShader(glGetUniformLocation(*program, "light.lPos"));
}

void InitShapes(ShaderParamsC* params)
{
	//create one unit sphere in the origin
	sphere = new SphereC(10, 10, 1);
	sphere->SetKa(glm::vec3(0.3, 0.3, 0.3));
	sphere->SetKs(glm::vec3(0, 0, 1));
	//sphere->SetKd(glm::vec3(0.7, 0.7, 0.7));
	sphere->SetSh(200);
	sphere->SetModel(glm::mat4(1.0));
	sphere->SetModelMatrixParamToShader(params->modelParameter);
	sphere->SetModelViewNMatrixParamToShader(params->modelViewNParameter);
	sphere->SetKaToShader(params->kaParameter);
	sphere->SetKdToShader(params->kdParameter);
	sphere->SetKsToShader(params->ksParameter);
	sphere->SetShToShader(params->shParameter);

	terrain = new TerrainC(heightmap);
	terrain->SetKa(glm::vec3(0.1, 0.1, 0.1));
	terrain->SetKs(glm::vec3(0, 0, 1));
	terrain->SetKd(glm::vec3(0.7, 0.7, 0.7));
	terrain->SetSh(200);
	terrain->SetModel(glm::mat4(1.0));
	terrain->SetModelMatrixParamToShader(params->modelParameter);
	terrain->SetModelViewNMatrixParamToShader(params->modelViewNParameter);
	terrain->SetKaToShader(params->kaParameter);
	terrain->SetKdToShader(params->kdParameter);
	terrain->SetKsToShader(params->ksParameter);
	terrain->SetShToShader(params->shParameter);

	for (int i = 0; i < num_particles; i++) {
		particles[i].loc.y = 15;
		particles[i].loc.x = (rand() % 140 + 10) / 6.f;
		particles[i].loc.z = (rand() % 140 + 10) / 6.f;
		particles[i].mass = (rand() % 5 + 1) / 5.0f;
		particles[i].color.x = rand() % 256 / 255.f;
		particles[i].color.y = rand() % 256 / 255.f;
		particles[i].color.z = rand() % 256 / 255.f;
	}

}

void setTerrain() {

	//terrain
	int channelCount;
	terrainData = stbi_load("src/terrain/dem.jpg", &t_width, &t_height, &channelCount, 3);
	if (terrainData == NULL) {
		cout << "Load Error\n";
		exit(1);
	}
	printf("Terrain: %d by %d\n", t_width, t_height);

	size_t t_size = t_width * t_height * channelCount;
	uint8_t * gray = new uint8_t[t_width * t_height];
	for (unsigned char* a = terrainData, *bnw = gray; a != terrainData + t_size; a += channelCount, bnw += 1) {
		*bnw = (uint8_t)((*a + *(a + 1) + *(a + 2)) / 3.0);
	}

	vector<GLfloat> heights;
	for (int i = 0; i < t_width; i++) {
		heights.clear();
		for (int j = 0; j < t_height; j++) {
			GLfloat temp = gray[i + j * t_width]/255.0f*5;
			heights.push_back(temp);
		}
		heightmap.push_back(heights);
	}
	
}

int main(int argc, char **argv)
{ 
	setTerrain();
  glutInitDisplayString("stencil>=2 rgb double depth samples");
  glutInit(&argc, argv);
  glutInitWindowSize(wWindow,hWindow);
  glutInitWindowPosition(500,100);
  window = glutCreateWindow("Model View Projection GLSL");
  GLenum err = glewInit();
  if (GLEW_OK != err){
   fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
  }
  createMenu();

  glutDisplayFunc(Display);
  glutIdleFunc(Idle);
  glutMouseFunc(Mouse);
  glutReshapeFunc(Reshape);
  glutKeyboardFunc(Kbd); //+ and -
  glutSpecialUpFunc(SpecKbdRelease); //smooth motion
  glutSpecialFunc(SpecKbdPress);
  InitializeProgram(&shaderProgram);
  InitShapes(&params);
  glutMainLoop();
  return 0;        
}
	
