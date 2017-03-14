#include <iostream>
#include "clipper.hpp"
#define WIN32
#include "glut.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <ctime>
#include <sstream>
#include <omp.h>

using namespace ClipperLib;
using namespace std;

//ќтвечает за масштаб. –азмер окна в единичных отрезках
const int X_COORD = 4500;
const int Y_COORD = 4500;
// оординаты центра осей координат
int x_c = X_COORD / 2;
int y_c = Y_COORD / 2;

struct FieldSpec {
	int X;
	int Y;
	int inner_rad;
	int external_rad;
	double angle_det;
	FieldSpec(int x, int y, int ir, int er, double a) : X(x), Y(y), inner_rad(ir), external_rad(er), angle_det(a) {};
};

typedef std::vector< FieldSpec > Fields;
inline Fields& operator <<(Fields& fields, const FieldSpec& s) { fields.push_back(s); return fields; }


void draw_axes()
{
	glColor3f(1.0, 1.0, 1.0);
	double dist = 10.0; //рассто€ние между соседними засечками
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
	for (double a = 0; a < 360; a += field.angle_det)
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

Paths getResultField1(Fields flist)
{
	int op = clock();
	Paths tmp, field, result;
	Clipper c;
	for (int i = 0; i < flist.size(); i++)
	{
		c.AddPaths(getField(flist[i]), ptSubject, true);
	}
	c.Execute(ctUnion, result, pftNonZero);
	cout << clock()-op << "========="<<endl;
	return result;
}

Paths getResultField(Fields & flist, int num_th, int fpt)
{

	int op = clock();
	vector<Paths> result;
	int field_per_thread = fpt; //200
	//field_per_thread = 100;
	omp_lock_t my_lock;
	omp_init_lock(&my_lock);

	int adding_parts = 0;
	int adding_final = 0;
	int exec_parts = 0;
	int push_time = 0;
	int exec_final = 0;
	int tmp_tm = 0;
	int before_step = clock();
#pragma omp parallel for shared(result) num_threads(num_th)
	for (int i =0; i <= flist.size() - field_per_thread; i += field_per_thread)
	{
		Clipper c;
		Paths tmp;
		tmp_tm = clock();
		for (int j = 0; j<field_per_thread; j++)
		{

			c.AddPaths(getField(flist[i + j]), ptSubject, true);
		}
		adding_parts += clock() - tmp_tm;

		tmp_tm = clock();
		c.Execute(ctUnion, tmp, pftNonZero);
		exec_parts += clock()-tmp_tm;
		omp_set_lock(&my_lock);
		result.push_back(tmp);
		omp_unset_lock(&my_lock);
	}
	int after_step = clock();
	Clipper c;
	Paths tmp;
	Paths final;
	//убрал
	tmp_tm = clock();
	for (int i = 0; i < result.size(); i++)
	{
		c.AddPaths(result[i], ptSubject, true);
	}
	adding_final = clock() - tmp_tm;
	tmp_tm = clock();
	c.Execute(ctUnion, final, pftNonZero);
	exec_final = clock() - tmp_tm;
	op = clock() - op;


	//cout << "\nfield was: " << flist.size() << ",field now: " <<  final.size() <<endl;
	//cout << "add_prts: " << adding_parts << endl; //врем€ на создание и добавление частей 1 потоком
	//cout << "add_fin: " << adding_final << endl; //добавление всего объедененного в путь
	//cout << "exec_prts: " << exec_parts << endl; //врем€ на объединение частей в 1 потоке
	//cout << "common_parts: " << after_step-before_step << endl; 
	//cout << "exec_fin: " << exec_final << endl; //финальное объединение
	//cout << "common time: " << op << endl;
	//cout << "------------";

	return final;
}

void test(Fields & f, int angle, int number_th, int number_test, int fpt)
{
	vector<int> tests;
	Paths result;

	for (int i = 0; i < number_test; i++) {
		unsigned int start_time = clock();
		result = getResultField(f, number_th, fpt);
		unsigned int end_time = clock();
		tests.push_back(end_time - start_time);
	}
	int average_time = 0;
	for (int i = 1; i < tests.size(); i++) // не учитываю первый запуск, потому что его врем€ слишком сильно выдел€етс€
	{
		average_time += tests[i];
	}
	cout << "Number field: " << f.size() << endl;
	cout << "Angle det: " << angle << endl;
	cout << "Num th: " << number_th << endl;
	cout << "Field per thread: " << fpt << endl;
	cout << "Num test: " << number_test << endl;
	cout << "-----------Duration: " << average_time / ((tests.size() - 1) * 1000.0) << endl;
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	draw_axes();
	Paths result;
	Fields f;

	double angle_det = 1.0;
	int number_thread = 4;
	int field_per_thread = 5000;
	int number_test = 4;



	//for (int i = -4000; i <= 4000; i += 40)
	//{
	//	for (int j = -4000; j <= 4000; j += 40)
	//	{
	//		f << FieldSpec(i, j, 25, 40, angle_det);
	//	}
	//}

	for (int i = -4000; i <= -3200; i += 40)//1
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i-100, j-100, 25, 40, angle_det);
		}
	}

	for (int i = -3200; i <= -2400; i += 40)//2
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i, j, 25, 40, angle_det);
		}
	}

	for (int i = -2400; i <= -1600; i += 40)//3
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+100, j+100, 25, 40, angle_det);
		}
	}

	for (int i = -1600; i <= -800; i += 40)//4
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+200, j+200, 25, 40, angle_det);
		}
	}

	for (int i = -800; i <= 0; i += 40)//5
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+300, j+300, 25, 40, angle_det);
		}
	}

	for (int i = 0; i <= 800; i += 40)//6
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+400, j+400, 25, 40, angle_det);
		}
	}

	for (int i = 800; i <= 1600; i += 40)//7
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+500, j+500, 25, 40, angle_det);
		}
	}

	for (int i = 1600; i <= 2400; i += 40)//8
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+700, j+700, 25, 40, angle_det);
		}
	}

	for (int i = 2400; i <= 3200; i += 40)//9
	{
		for (int j = -4000; j <= 4000; j += 40)
		{
			f << FieldSpec(i+800, j+800, 25, 40, angle_det);
		}
	}

	
	test(f, angle_det, number_thread, number_test, field_per_thread);

	/*for (field_per_thread; field_per_thread < 1000; field_per_thread+=150) {

		test(f, angle_det, number_thread, number_test, field_per_thread);
	}*/

	//f.clear();
	//f << FieldSpec(0, 0, 60, 80) << FieldSpec(100, 0, 60, 80) << FieldSpec(50, 30, 60, 80);
	drawFields(f);
	


	//drawSolution(result);
	//ostringstream out;
	//glColor3f(0.0, 0.0, 0.0);
	//glBegin(GL_POLYGON);
	//glVertex2i(0, 500);
	//glVertex2i(210, 500);
	//glVertex2i(210, 450);
	//glVertex2i(0, 450);
	//glEnd();
	////cout << "Duration: " << (end_time - start_time)/1000.0<<endl;
	//renderBitmapString(3, 480, out.str().c_str(), 0);
	//out.str(std::string());
	////cout << "Number of field: " << f.size();
	//renderBitmapString(3, 465, out.str().c_str(), 0);
	//
	

	
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