#include "stdafx.h"
#include "LLMRunner.h"
#include <QDebug>
#include <vector>
#include <cstring>

LLMRunner::LLMRunner(const QString& modelPath, QObject* parent)
	: QObject(parent), m_modelPath(modelPath) {
}

LLMRunner::~LLMRunner() {
	if (m_ctx) llama_free(m_ctx);
	if (m_model) llama_model_free(m_model);
	llama_backend_free();
}

void LLMRunner::init() {
	llama_backend_init();

	llama_model_params mparams = llama_model_default_params();
	m_model = llama_model_load_from_file(m_modelPath.toUtf8().constData(), mparams);
	if (!m_model) {
		qWarning() << "Failed to load model";
		return;
	}

	llama_context_params cparams = llama_context_default_params();
	cparams.n_ctx = 2048;
	m_ctx = llama_init_from_model(m_model, cparams);
	if (!m_ctx) {
		qWarning() << "Failed to create context";
		llama_model_free(m_model);
		m_model = nullptr;
		return;
	}

	m_vocab = llama_model_get_vocab(m_model);
	emit signalInitLLMFinished();
}

QString LLMRunner::buildPrompt(const QString& prompt) {
	QString conv;
	for (const auto& p : m_history)
		conv += p.first + ": " + p.second + "\n";
	conv += "User: " + prompt + "\nAssistant:";
	return conv;
}

QString LLMRunner::chat(const QString& prompt) {
	QMutexLocker locker(&m_mutex);
	if (!m_model || !m_ctx || !m_vocab) return "模型未加载";

	// 新一轮生成前清掉中断标志
	m_abort.store(false);

	const QString promptFull = buildPrompt(prompt);
	const std::string input = promptFull.toUtf8().constData();

	std::vector<llama_token> tokens(input.size() * 4 + 4);
	const int n_tokens = llama_tokenize(
		m_vocab,
		input.c_str(), (int)input.size(),
		tokens.data(), (int)tokens.size(),
		/*add_special*/ true,
		/*parse_special*/ false
	);

	if (n_tokens <= 0) {
		qWarning() << "Tokenize失败：" << input.c_str();
		emit chatResult(QStringLiteral("输入解析失败"));
		return "输入解析失败";
	}
	tokens.resize(n_tokens);

	// 清理 kv 缓存（重要：避免上下文脏数据）
	llama_memory_clear(llama_get_memory(m_ctx), true);

	// 先送入 prompt
	llama_batch batch = llama_batch_init((int)tokens.size(), 0, 1);
	for (int i = 0; i < (int)tokens.size(); ++i) {
		batch.token[i] = tokens[i];
		batch.pos[i] = i;
		batch.n_seq_id[i] = 1;
		batch.seq_id[i][0] = 0;
	}
	batch.n_tokens = (int)tokens.size();

	if (llama_decode(m_ctx, batch) != 0) {
		llama_batch_free(batch);
		emit chatResult(QStringLiteral("模型解码失败"));
		return "模型解码失败";
	}
	llama_batch_free(batch);

	int n_past = (int)tokens.size();
	const int n_max_tokens = 256;   // 可按需调大
	QString result;

	for (int i = 0; i < n_max_tokens; ++i) {
		if (m_abort.load()) {
			// 被请求中断——发出当前已有文本并收尾
			emit chatResult(result.trimmed());
			return result.trimmed();
		}

		float* logits = llama_get_logits(m_ctx);
		const int vocab_size = llama_vocab_n_tokens(m_vocab);
		if (!logits || vocab_size <= 0) {
			qWarning() << "logits为空 或 vocab_size 无效";
			emit chatResult(QStringLiteral("模型输出异常"));
			return "模型输出异常";
		}

		// 简单贪心采样（可替换为 top-p / 温度采样）
		int max_idx = 0;
		float max_val = logits[0];
		for (int j = 1; j < vocab_size; ++j) {
			if (logits[j] > max_val) {
				max_val = logits[j];
				max_idx = j;
			}
		}

		const llama_token new_token = (llama_token)max_idx;
		if (new_token == llama_vocab_eos(m_vocab))
			break;

		char piece[512] = { 0 };
		llama_token_to_piece(m_vocab, new_token, piece, (int)sizeof(piece),
			/*lstrip*/ 0, /*special*/ true);
		const QString pieceStr = QString::fromUtf8(piece);

		// 1) 累积到最终文本
		result += pieceStr;
		// 2) 立刻把这次片段“流”出去
		emit chatStreamResult(pieceStr);

		// 把新 token 继续送入
		llama_batch nextBatch = llama_batch_init(1, 0, 1);
		nextBatch.token[0] = new_token;
		nextBatch.pos[0] = n_past;
		nextBatch.n_seq_id[0] = 1;
		nextBatch.seq_id[0][0] = 0;
		nextBatch.n_tokens = 1;

		if (llama_decode(m_ctx, nextBatch) != 0) {
			llama_batch_free(nextBatch);
			emit chatResult(result.trimmed());
			return result.trimmed();
		}
		llama_batch_free(nextBatch);
		++n_past;
	}

	// 记录对话历史
	m_history.append({ "User", prompt });
	m_history.append({ "Assistant", result.trimmed() });

	qDebug() << "[LLM 输出]:" << result.trimmed();

	// 最终收尾
	emit chatResult(result.trimmed());
	return result.trimmed();
}
