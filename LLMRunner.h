#ifndef LLMRUNNER_H
#define LLMRUNNER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <llama.h>

class LLMRunner : public QObject
{
	Q_OBJECT
public:
	explicit LLMRunner(const QString& modelPath, QObject* parent = nullptr);
	~LLMRunner();

	QString chat(const QString& prompt);
	void appendHistory(const QString& role, const QString& content);
	void clearHistory();

signals:
	void chatResult(const QString& result);

private:
	QString buildPrompt(const QString& prompt);

	llama_model* m_model = nullptr;
	llama_context* m_ctx = nullptr;
	const llama_vocab* m_vocab = nullptr;
	QString m_modelPath;
	QVector<QPair<QString, QString>> m_history;
	QMutex m_mutex;
};

#endif // LLMRUNNER_H
