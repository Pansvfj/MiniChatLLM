#include "stdafx.h"
#include <QApplication>
#include "ChatWindow.h"
#include "ModelSelectDialog.h"

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	ModelSelectDialog dlg;
	if (dlg.exec() != QDialog::Accepted) {
		return 0; // È¡ÏûÆô¶¯
	}
	if (!g_onlineInfer) {
		const QString modelPath = dlg.modelPath();
		ChatWindow *win = new ChatWindow(modelPath);
		win->resize(600, 500);
		win->show();
	}

	return app.exec();
}
