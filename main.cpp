#include "stdafx.h"
#include <QApplication>
#include "ChatWindow.h"

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	ChatWindow win;
	win.resize(600, 500);
	win.show();

	return app.exec();
}
