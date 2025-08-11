#ifndef MODELSELECTDIALOG_H
#define MODELSELECTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class ModelSelectDialog : public QDialog {
	Q_OBJECT
public:
	explicit ModelSelectDialog(QWidget* parent = nullptr);
	QString modelPath() const;

private slots:
	void onBrowse();
	void onTextChanged(const QString&);

private:
	QLineEdit* m_edit;
	QPushButton* m_ok;
};

#endif // MODELSELECTDIALOG_H
