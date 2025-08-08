#ifndef LLMRUNNER_H
#define LLMRUNNER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <llama.h>

// LLMRunner.h
#include <atomic>
class LLMRunner : public QObject {
	Q_OBJECT
public:
	explicit LLMRunner(const QString& modelPath, QObject* parent = nullptr);
	~LLMRunner();
	void init();
	QString chat(const QString& prompt);

	// 新增：请求中断
	void requestAbort() { m_abort.store(true); }

signals:
	void chatResult(const QString& result);
	void signalInitLLMFinished();

private:
	QString buildPrompt(const QString& prompt);

	llama_model* m_model = nullptr;
	llama_context* m_ctx = nullptr;
	const llama_vocab* m_vocab = nullptr;
	QString m_modelPath;
	QVector<QPair<QString, QString>> m_history;
	QMutex m_mutex;

	std::atomic_bool m_abort{ false };
};


#endif // LLMRUNNER_H
