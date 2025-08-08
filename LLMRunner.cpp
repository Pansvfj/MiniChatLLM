#include "stdafx.h"
#include "LLMRunner.h"
#include <QDebug>
#include <vector>
#include <cstring>

LLMRunner::LLMRunner(const QString& modelPath, QObject* parent)
	: QObject(parent), m_modelPath(modelPath)
{

}

LLMRunner::~LLMRunner()
{
	if (m_ctx) llama_free(m_ctx);
	if (m_model) llama_model_free(m_model);
	llama_backend_free();
}

void LLMRunner::init() {
	llama_backend_init();

	llama_model_params mparams = llama_model_default_params();
	m_model = llama_model_load_from_file(m_modelPath.toUtf8().constData(), mparams);
	if (!m_model) { qWarning() << "Failed to load model"; return; }

	llama_context_params cparams = llama_context_default_params();
	cparams.n_ctx = 2048;
	// ★ 允许外部取消
	cparams.abort_callback = [](void* ud)->bool {
		return reinterpret_cast<std::atomic_bool*>(ud)->load();
		};
	cparams.abort_callback_data = &m_abort;

	m_ctx = llama_init_from_model(m_model, cparams);
	if (!m_ctx) { qWarning() << "Failed to create context"; llama_model_free(m_model); m_model = nullptr; return; }

	m_vocab = llama_model_get_vocab(m_model);
	emit signalInitLLMFinished();
}

QString LLMRunner::buildPrompt(const QString& prompt)
{
	QString conv;
	for (const auto& p : m_history)
		conv += p.first + ": " + p.second + "\n";
	conv += "User: " + prompt + "\nAssistant:";
	return conv;
}

QString LLMRunner::chat(const QString& prompt)
{
	QMutexLocker locker(&m_mutex);
	if (!m_model || !m_ctx || !m_vocab) return "模型未加载";

	QString promptFull = buildPrompt(prompt);
	std::string input = promptFull.toUtf8().constData();

	std::vector<llama_token> tokens(input.size() * 4 + 4);
	int n_tokens = llama_tokenize(
		m_vocab,
		input.c_str(), input.size(),
		tokens.data(), tokens.size(),
		true,  // add_special
		false  // parse_special
	);

	if (n_tokens <= 0) {
		qWarning() << "Tokenize失败：" << input.c_str();
		return "输入解析失败";
	}

	tokens.resize(n_tokens);

	llama_memory_clear(llama_get_memory(m_ctx), true);

	llama_batch batch = llama_batch_init(tokens.size(), 0, 1);
	for (int i = 0; i < tokens.size(); ++i) {
		batch.token[i] = tokens[i];
		batch.pos[i] = i;
		batch.n_seq_id[i] = 1;
		batch.seq_id[i][0] = 0;
	}
	batch.n_tokens = tokens.size();

	llama_decode(m_ctx, batch);
	llama_batch_free(batch);

	int n_past = tokens.size();
	int n_max_tokens = 256; //越大越耗时
	QString result;

	for (int i = 0; i < n_max_tokens; ++i) {

		if (m_abort.load()) {
			qWarning() << "推理被中断";
			break;
		}

		float* logits = llama_get_logits(m_ctx);
		int vocab_size = llama_vocab_n_tokens(m_vocab);

		if (!logits || vocab_size <= 0) {
			qWarning() << "logits为空 或 vocab_size 无效";
			return "模型输出异常";
		}

		int max_idx = 0;
		float max_val = logits[0];
		for (int j = 1; j < vocab_size; ++j) {
			if (logits[j] > max_val) {
				max_val = logits[j];
				max_idx = j;
			}
		}

		llama_token new_token = max_idx;
		if (new_token == llama_vocab_eos(m_vocab))
			break;

		char piece[512] = { 0 };
		llama_token_to_piece(m_vocab, new_token, piece, sizeof(piece), 0, true);
		result += QString::fromUtf8(piece);

		llama_batch nextBatch = llama_batch_init(1, 0, 1);
		nextBatch.token[0] = new_token;
		nextBatch.pos[0] = n_past;
		nextBatch.n_seq_id[0] = 1;
		nextBatch.seq_id[0][0] = 0;
		nextBatch.n_tokens = 1;

		llama_decode(m_ctx, nextBatch);
		llama_batch_free(nextBatch);

		n_past += 1;

		// 注释掉以下代码避免误终止
		// if (result.endsWith("User:") || result.endsWith("<|im_end|>"))
		//     break;
	}

	qDebug() << "[LLM 输出]:" << result.trimmed();

	emit chatResult(result.trimmed());
	return result.trimmed();
}
