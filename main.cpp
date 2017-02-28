#include <iostream>
#include "clipper.hpp"
#include <glut.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <ctime>
#include <sstream>
#include <omp.h>


using namespace ClipperLib;
using namespace std;

//Отвечает за масштаб. Размер окна в единичных отрезках
const int X_COORD = 500;
const int Y_COORD = 500;
//Координаты центра осей координат
int x_c = X_COORD / 2;
int y_c = Y_COORD / 2;

struct FieldSpec {
	int X;
	int Y;
	int inner_rad;
	int external_rad;
	int angle_det;
	FieldSpec(int x, int y, int ir, int er, int a=1) : X(x), Y(y), inner_rad(ir), external_rad(er), angle_det(a) {};
};

typedef std::vector< FieldSpec > Fields;
inline Fields& operator <<(Fields& fields, const FieldSpec& s) { fields.push_back(s); return fields; }


void draw_axes()
{
	glColor3f(1.0, 1.0, 1.0);
	double dist = 10.0; //расстояние между соседними засечками
	double lenght = 1.0; //длина засечки

	glBegin(GL_LINES);
	//рисуем горизонтальную ось
	glVertex2f(0.0, Y_COORD / 2);
	glVertex2f(X_COORD, Y_COORD / 2);
	//рисуем засечки
	for (int i = 0.0; i < X_COORD; i += dist)
	{
		glVertex2f(i, (Y_COORD / 2) - lenght / 2);
		glVertex2f(i, (Y_COORD / 2) + lenght / 2);
	}

	//рисуем вертикальную ось
	glVertex2f(X_COORD / 2, 0.0);
	glVertex2f(X_COORD / 2, Y_COORD);
	//рисуем засечки
	for (int j = 0.0; j < Y_COORD; j += dist)
	{
		glVertex2f((X_COORD / 2) - (lenght + 0.1) / 2, j);
		glVertex2f((X_COORD / 2) + (lenght / 2), j);
	}
	glEnd();

}

void drawPolygon(Path p, int color, int fill_mode=0) {

	if (color == 0 && fill_mode==0) glColor3f(0.0, 1.0, 0.0);
	if(color == 1 && fill_mode == 0) glColor3f(1.0, 0.0, 0.0);
	if (color == 0 && fill_mode == 1) glColor3f(0.741, 0.718, 0.420);
	if (color == 1 && fill_mode == 1) glColor3f(1.0, 1.0, 1.0);
	if (fill_mode == 0)
	{
		glBegin(GL_LINE_LOOP);
		for (int i = 0; i < p.size(); i++)
		{
			glVertex2i(x_c + p[i].X, y_c + p[i].Y);
		}
	}
	else {
		glBegin(GL_LINE_LOOP);
		for (int i = 0; i < p.size(); i++)
		{
			glVertex2i(x_c + p[i].X, y_c + p[i].Y);
		}
	}

	glEnd();
}

void drawField(Paths p)
{
	drawPolygon(p[0], 0);
	drawPolygon(p[1], 1);
}

Paths getField(FieldSpec field) {
	
	Path external, inner;
	for (int a = 0; a < 360; a += field.angle_det)
	{
		external << IntPoint((int)(field.X + field.external_rad*cos(a*M_PI/180)), (int)(field.Y + field.external_rad*sin(a*M_PI / 180)));
		inner << IntPoint((int)(field.X + field.inner_rad*cos(a*M_PI / 180)), (int)(field.Y + field.inner_rad*sin(a*M_PI / 180)));
	}
	ReversePath(external);
	return Paths() << external << inner;
}

void drawSolution(Paths p)
{
	for (int i = 0; i < p.size(); i++)
	{
		if (Orientation(p[i]))
		{
			drawPolygon(p[i], 0, 1);
		}
	}
	for (int i = 0; i < p.size(); i++)
		{
			if (!Orientation(p[i]))
			{
				drawPolygon(p[i], 1, 1);
			}
		}

}

void drawFields(Fields flist)
{
	for (int i = 0; i < flist.size(); i++)
	{
		drawField(getField(flist[i]));
	}
}

void renderBitmapString(float x, float y, const char *string, int mode_color)
{
	switch (mode_color)
	{
	case 0: glColor3f(1.0, 1.0, 1.0);
		break;
	case 1: glColor3f(1.0, 0.0, 0.0);
		break;
	case 2: glColor3f(1.0, 0.7, 0.0);
		break;
	}
	const char *c;
	void *font = GLUT_BITMAP_9_BY_15;
	glRasterPos2f(x, y);
	for (c = string; *c != '\0'; c++) {
		glutBitmapCharacter(font, *c);
	}
}

//Paths getResultField(Fields flist)
//{
//	Paths tmp, field, result;
//	Clipper c;
//	for (int i = 0; i < flist.size(); i++)
//	{
//		c.AddPaths(getField(flist[i]), ptSubject, true);
//	}
//	c.Execute(ctUnion, result, pftNonZero);
//	return result;
//}

Paths getResultField(Fields flist)
{

	vector<Paths> result;
	int field_per_thread = 200;
	omp_lock_t my_lock;
	omp_init_lock(&my_lock);
#pragma omp parallel for shared(result)  num_threads(2)
	for (int i =0; i < flist.size() - field_per_thread; i += field_per_thread)
	{
		Clipper c;
		Paths tmp;
		for (int j = 0; j<field_per_thread; j++)
		{
			c.AddPaths(getField(flist[i + j]), ptSubject, true);
		}
		c.Execute(ctUnion, tmp, pftNonZero);
		omp_set_lock(&my_lock);
		result.push_back(tmp);
		omp_unset_lock(&my_lock);
	}
	Clipper c;
	Paths tmp;
	Paths final;
	for (int i= flist.size() - field_per_thread; i < flist.size(); i++)
	{
		c.AddPaths(getField(flist[i]), ptSubject, true);
	}
	for (int i = 0; i < result.size(); i++)
	{
		c.AddPaths(result[i], ptSubject, true);
	}
	c.Execute(ctUnion, final, pftNonZero);

	return final;
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	draw_axes();
	Paths result;
	Fields f;
	f << FieldSpec(0, 0, 30, 45) << FieldSpec(100, 30, 30, 45) << FieldSpec(-200, -40, 40, 70) << FieldSpec(100, -60, 25, 80) << 
		FieldSpec(-10, -250, 25, 50) <<	FieldSpec(20, 10, 10, 25) << FieldSpec(150, 45, 30, 45) << FieldSpec(-180, -40, 40, 70) << 
		FieldSpec(160, -100, 65, 80) << FieldSpec(-15, -220, 35, 70) << FieldSpec(-100, -250, 25, 50) << FieldSpec(200, 10, 15, 25) << 
		FieldSpec(15, 145, 30, 45) << FieldSpec(-18, -140, 40, 70) <<
		FieldSpec(160, -10, 65, 80) << FieldSpec(-150, -22, 35, 70);
	for (int i = -250; i <= 250; i += 30)
	{
		for (int j = -250; j <= 250; j += 35)
		{
			f << FieldSpec(i, j, 45, 50);
		}
	}
	//drawFields(f);
	unsigned int start_time = clock();
	result = getResultField(f);
	unsigned int end_time = clock();
	drawSolution(result);

	ostringstream out;
	glColor3f(0.0, 0.0, 0.0);
	glBegin(GL_POLYGON);
	glVertex2i(0, 500);
	glVertex2i(210, 500);
	glVertex2i(210, 450);
	glVertex2i(0, 450);
	glEnd();
	out << "Duration: " << (end_time - start_time)/1000.0;
	renderBitmapString(3, 480, out.str().c_str(), 0);
	out.str(std::string());
	out << "Number of field: " << f.size();
	renderBitmapString(3, 465, out.str().c_str(), 0);
	
	

	
	glFlush();
}


int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize(500, 500);
	glutInitWindowPosition(800, 200);
	glutCreateWindow("RTV Field Test");

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//пространство координат
	glOrtho(0.0, X_COORD, 0.0, Y_COORD, -1.0, 1.0);

	glutDisplayFunc(display);
	glutMainLoop();
}