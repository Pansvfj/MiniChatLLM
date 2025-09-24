#include "OnlineInfetWindow.h"
#include <QNetworkRequest>
#include <QTextBrowser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QDebug>

OnlineInfetWindow::OnlineInfetWindow(const QString& url, QWidget* parent)
	: QWidget(parent),
	manager(new QNetworkAccessManager(this)),
	serverUrl(url),
	bufferPos(0),
	replyFinished(false)
{
	QTextBrowser* textBrowser = new QTextBrowser(this);
	textBrowser->setObjectName("textBrowser");
	textBrowser->setReadOnly(true);

	QLineEdit* lineEdit = new QLineEdit(this);
	lineEdit->setObjectName("lineEdit");

	QPushButton* sendButton = new QPushButton("Send", this);
	sendButton->setObjectName("sendButton");

	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(textBrowser);

	QHBoxLayout* inputLayout = new QHBoxLayout;
	inputLayout->addWidget(lineEdit);
	inputLayout->addWidget(sendButton);

	mainLayout->addLayout(inputLayout);
	setLayout(mainLayout);

	connect(sendButton, &QPushButton::clicked, this, &OnlineInfetWindow::onSendButtonClicked);
	connect(lineEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

	// ��ʱ�����������ʾ������ÿ�ַ� append��ʹ�� insertPlainText��
	displayTimer = new QTimer(this);
	connect(displayTimer, &QTimer::timeout, this, &OnlineInfetWindow::updateBotReply);
	displayTimer->start(40); // ÿ40ms������ʾ������

	lineEdit->setFocus();
}

void OnlineInfetWindow::onSendButtonClicked()
{
	QLineEdit* lineEdit = findChild<QLineEdit*>("lineEdit");
	QString text = lineEdit->text().trimmed();
	if (text.isEmpty()) return;

	QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
	textBrowser->append("<b>You:</b> " + text);
	lineEdit->clear();

	// ���û���״̬
	botReplyBuffer.clear();
	bufferPos = 0;
	replyFinished = false;

	// �� UI �в��� AI �����ǩ������ʹ�� insertPlainText ׷�ӣ�
	textBrowser->append("<b>AI:</b> ");

	// ��������
	sendRequest(text);
}

void OnlineInfetWindow::sendRequest(const QString& text)
{
	QJsonObject jsonRequest;
	QJsonArray messagesArray;
	QJsonObject message;
	message["role"] = "user";
	message["content"] = text;
	messagesArray.append(message);
	jsonRequest["messages"] = messagesArray;

	// �� max_tokens ��ߣ���֤�����ɸ����ı�����ģ������������
	jsonRequest["max_tokens"] = 4096;

	QJsonDocument jsonDoc(jsonRequest);
	QByteArray requestData = jsonDoc.toJson(QJsonDocument::Compact);

	QNetworkRequest request;
	request.setUrl(QUrl(serverUrl));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply* reply = manager->post(request, requestData);

	// ��ʼ���ۻ�����
	replyBufferMap.insert(reply, QByteArray());

	// �� readyRead�������ۻ����� finished��������ɣ�
	connect(reply, &QNetworkReply::readyRead, this, &OnlineInfetWindow::onReplyReadyRead);
	connect(reply, &QNetworkReply::finished, this, &OnlineInfetWindow::onReplyFinished);

	qDebug() << "Request sent (len=" << requestData.size() << "):" << requestData.left(512);
}

void OnlineInfetWindow::onReplyReadyRead()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	QByteArray chunk = reply->readAll();
	if (chunk.isEmpty()) return;

	// ���� chunk �ۻ�����Ӧ reply �� buffer �У������Ե�������
	replyBufferMap[reply].append(chunk);

	// ��ѡ��Ϊ�˵��ԣ���ӡǰ��һС��
	qDebug() << "readyRead chunk len=" << chunk.size() << " preview:" << chunk.left(256);
}

void OnlineInfetWindow::onReplyFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	// ȡ���ۻ���������Ӧ body��readyRead �ڼ��ۻ���
	QByteArray full = replyBufferMap.value(reply);
	if (full.isEmpty()) full = reply->readAll();
	replyBufferMap.remove(reply);

	if (reply->error() == QNetworkReply::NoError) {
		qDebug() << "Server Response (full len=" << full.size() << "):" << full.left(2048);

		QJsonParseError err;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(full, &err);
		if (err.error == QJsonParseError::NoError && jsonDoc.isObject()) {
			QJsonObject jsonResponse = jsonDoc.object();

			if (jsonResponse.contains("choices")) {
				QJsonArray choices = jsonResponse.value("choices").toArray();
				if (!choices.isEmpty()) {
					// �����һ����ݶ��ֽṹ
					QJsonValue first = choices.at(0);
					QString raw;

					if (first.isObject()) {
						QJsonObject choiceObj = first.toObject();

						// ���� text �ֶΣ���Щʵ��ʹ�� text��
						if (choiceObj.contains("text")) {
							raw = choiceObj.value("text").toString();
						}
						// �ٳ��� message.content��OpenAI-style��
						else if (choiceObj.contains("message")) {
							QJsonValue msgVal = choiceObj.value("message");
							if (msgVal.isObject()) {
								QJsonObject msgObj = msgVal.toObject();
								raw = msgObj.value("content").toString();
							}
							else if (msgVal.isString()) {
								raw = msgVal.toString();
							}
						}
						// �����������ܵ��ֶ�
						else {
							// ���԰����� choiceObj ת���ַ�������ѡ��
							raw = QJsonDocument(choiceObj).toJson(QJsonDocument::Compact);
						}
					}
					else if (first.isString()) {
						raw = first.toString();
					}
					else {
						raw = QString::fromUtf8(full); // ����
					}

					botReplyBuffer = cleanAndUnescape(raw);
				}
				else {
					qDebug() << "No choices in response JSON.";
					botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
				}
			}
			else {
				// �Ǳ�׼�ṹ�����׳���ֱ�Ӱ� body ����Ϊ�ı�
				botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
			}
		}
		else {
			// ���� JSON�������Ǵ��ı�����ʽ���������ı�����
			qDebug() << "JSON parse error:" << err.errorString() << ", treating full body as text.";
			botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
		}
	}
	else {
		qDebug() << "Network error:" << reply->errorString();
		QByteArray resp = full;
		qDebug() << "Error response body:" << resp.left(1024);
		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->append("<span style='color:red;'><b>AI Error:</b> " + reply->errorString() + "</span>");
		botReplyBuffer = QString("\n[Error body]\n") + QString::fromUtf8(resp);
	}

	// ����������ɣ���ʾ���Ķ�ʱ����ѻ�����ʾ����������ʾ���ʱ��Ϊ��������
	replyFinished = true;
	reply->deleteLater();
}


void OnlineInfetWindow::updateBotReply()
{
	// �������������绹û��ɣ�����ʾ
	if (botReplyBuffer.isEmpty() && !replyFinished) return;

	// ÿ��׷��һС�飬����Ƶ�� UI �������ɵ��� chunkSize��
	const int chunkSize = 12;
	if (bufferPos < botReplyBuffer.length()) {
		int take = qMin(chunkSize, botReplyBuffer.length() - bufferPos);
		QString piece = botReplyBuffer.mid(bufferPos, take);
		bufferPos += take;

		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->moveCursor(QTextCursor::End);
		textBrowser->insertPlainText(piece);
		textBrowser->moveCursor(QTextCursor::End);
	}

	// �������ѽ����һ�����ȫ����ʾ => �ظ���ȫ����
	if (replyFinished && bufferPos >= botReplyBuffer.length()) {
		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->append(""); // ��ӻ��ж��䣬׼����һ������

		qDebug() << "Bot reply fully received and displayed.";
		// ����״̬��׼����һ�λỰ������� UI��
		botReplyBuffer.clear();
		bufferPos = 0;
		replyFinished = false;
	}
}

// ��ϴ�����룺�Ƴ� <|...|> ��ǩ���ѳ���ת������ (\xHH, \uXXXX, \n, \t, \\) ����Ϊ��ʵ�ַ�
QString OnlineInfetWindow::cleanAndUnescape(const QString& raw)
{
	if (raw.isEmpty()) return QString();

	// 1) �Ƴ����� <|...|> �ı�ǣ���̰����
	QString s = raw;
	QRegularExpression tagRe("<\\|.*?\\|>");
	s.remove(tagRe);

	// 2) �Ƴ���������ָ���������У����� leftover markers like `---` or similar (��ѡ)
	// s.remove(QRegularExpression("[-]{3,}"));

	// 3) ���� C ���ת�壺������ת�� \n \t \\ \xHH \uHHHH ��
	QString out;
	out.reserve(s.length());

	const QChar backslash = '\\';
	for (int i = 0; i < s.length(); ++i) {
		QChar c = s.at(i);
		if (c == backslash && i + 1 < s.length()) {
			QChar n = s.at(i + 1);
			if (n == 'n') { out.append('\n'); i += 1; continue; }
			if (n == 't') { out.append('\t'); i += 1; continue; }
			if (n == 'r') { out.append('\r'); i += 1; continue; }
			if (n == '\\') { out.append('\\'); i += 1; continue; }
			if (n == '"') { out.append('"'); i += 1; continue; }
			if (n == '\'') { out.append('\''); i += 1; continue; }

			// \xHH
			if (n == 'x' && i + 3 < s.length()) {
				bool ok = true;
				QString hex = s.mid(i + 2, 2);
				uchar byte = static_cast<uchar>(hex.toUInt(&ok, 16));
				if (ok) {
					// �� UTF-8 ��ʽ�����ֽڲ���
					QByteArray b;
					b.append(static_cast<char>(byte));
					out += QString::fromUtf8(b);
					i += 3;
					continue;
				}
			}

			// \uHHHH
			if (n == 'u' && i + 5 < s.length()) {
				QString hex = s.mid(i + 2, 4);
				bool ok = false;
				uint code = hex.toUInt(&ok, 16);
				if (ok) {
					out.append(QChar(static_cast<ushort>(code)));
					i += 5;
					continue;
				}
			}

			// Ĭ�ϣ�������б�ܣ����������ַ�
			out.append(n);
			i += 1;
			continue;
		}
		else {
			out.append(c);
		}
	}

	// 4) �������һ��ȥ�����ܲ����� tags������һ��
	out.remove(QRegularExpression("<\\|.*?\\|>"));

	return out;
}
