#include "stdafx.h"
#include "ModelSelectDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>

#include "OnlineInferWindow.h"

ModelSelectDialog::ModelSelectDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("select model file(*.gguf)"));
	setModal(true);
	resize(520, 120);

	QPushButton* pbSelectOnline = new QPushButton("Use Online infer", this);
	connect(pbSelectOnline, &QPushButton::clicked, this, [=]() {
		g_onlineInfer = true;
		QString serverUrl = IPAdress;
		OnlineInferWindow* window = new OnlineInferWindow(serverUrl);
		window->setWindowTitle("Online Interaction Window");
		window->resize(800, 600);
		window->show();
		QDialog::accept();
	});

	auto* lbl = new QLabel(tr("model file path:"), this);
	m_edit = new QLineEdit(this);
	auto* btn = new QPushButton((tr("brow")), this);

	// 记住上次选择
	QSettings s("MiniChatLLM", "MiniChatLLM");
	qDebug() << s.fileName();
	m_edit->setText(s.value("lastModelPath").toString());

	auto* row = new QHBoxLayout();
	row->addWidget(lbl);
	row->addWidget(m_edit, 1);
	row->addWidget(btn);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_ok = buttons->button(QDialogButtonBox::Ok);
	m_ok->setEnabled(QFileInfo::exists(m_edit->text()));

	auto* col = new QVBoxLayout(this);
	col->addLayout(row);
	col->addWidget(buttons);
	col->addWidget(pbSelectOnline);

	connect(btn, &QPushButton::clicked, this, &ModelSelectDialog::onBrowse);
	connect(m_edit, &QLineEdit::textChanged, this, &ModelSelectDialog::onTextChanged);
	connect(buttons, &QDialogButtonBox::accepted, this, [this] {
		// 存储最近一次选择
		QSettings s("MiniChatLLM", "MiniChatLLM");
		s.setValue("lastModelPath", m_edit->text());
		accept();
		qDebug() << s.fileName() << "save path:" << m_edit->text();
		});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ModelSelectDialog::modelPath() const {
	return m_edit->text().trimmed();
}

void ModelSelectDialog::onBrowse() {
	const QString file = QFileDialog::getOpenFileName(
		this,
		QStringLiteral("select GGUF format model file"),
		m_edit->text(),
		QStringLiteral("GGUF(*.gguf);;all files(*)"));
	if (!file.isEmpty()) m_edit->setText(file);
}

void ModelSelectDialog::onTextChanged(const QString& t) {
	m_ok->setEnabled(QFileInfo::exists(t));
}
