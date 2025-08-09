# MiniChatLLM

一个基于 **Qt + llama.cpp** 的本地大语言模型（LLM）桌面聊天 Demo。  
支持 **Qwen-1.5** 和 **Yi-1.5**（GGUF 格式）模型，提供基础推理和增强采样模式。

---

## ✨ 功能特点

- **本地推理** — 基于 [llama.cpp](https://github.com/ggerganov/llama.cpp)，无需联网即可运行。
- **Qt 图形界面** — 输入问题并显示回复。
- **模型加载进度提示** — 带有已用秒数的加载对话框。
- **两种推理后端**：
  - `LLMRunner` — 简单贪心解码。
  - `LLMRunnerYi` — Top-p + Temperature 采样，适配 Yi-1.5-6B-Chat。
- **流式输出** — 实时输出生成片段。
- **多线程执行** — 推理在后台线程运行，保证界面流畅。
- **对话历史记录** — 自动保留上下文。

---

## 📂 项目结构

```
MiniChatLLM/
├── ChatWindow.h / ChatWindow.cpp     # 主聊天界面
├── LLMRunner.h / LLMRunner.cpp       # Qwen / 简单解码 LLM 后端
├── LLMRunnerYi.h / LLMRunnerYi.cpp   # Yi-1.5 专用采样后端
├── LoadingTipWidget.h / .cpp         # 模型加载提示控件
├── stdafx.h / stdafx.cpp             # 公共头文件与工具函数
└── models/                           # 存放 .gguf 模型文件
```

---

## 🛠 依赖环境

- **C++17** 或更高版本
- [Qt 5.15+](https://www.qt.io/download-qt-installer)
- [llama.cpp (2024.06+)](https://github.com/ggerganov/llama.cpp) 编译后的库文件  
- 一个 **GGUF** 模型文件（如 Qwen1.5-0.5B-Chat-Q4_0.gguf、Yi-1.5-6B-Chat-Q4_K_M.gguf）

---

## ⚙️ 编译步骤

1. **克隆项目**  
   ```bash
   git clone https://github.com/yourname/MiniChatLLM.git
   cd MiniChatLLM
   ```

2. **编译 llama.cpp** 并拷贝头文件与库文件到项目中：  
   ```bash
   cmake -B build -DLLAMA_BUILD=OFF
   cmake --build build --config Release
   ```
   放置：
   - `llama.h` 到 `llama/include`
   - `llama.lib` / `llama.dll`（Windows）或 `.a` / `.so`（Linux）到 `llama/lib`

3. **使用 Qt Creator / Visual Studio 打开工程**  
   - 确保已链接 `llama` 库路径。
   - 设置为 **x64** 构建模式。
   - 修改 `ChatWindow.cpp` 中模型路径为你本地 `.gguf` 文件路径。

4. **编译并运行**  
   首次运行会加载模型（耗时取决于模型大小和硬件性能）。

---

## 🚀 使用方法

1. 启动程序。
2. 等待 **“加载模型中...”** 对话框完成。
3. 在输入框输入问题。
4. 按 **Enter** 或点击 **发送**。
5. AI 回复会显示在聊天窗口。

---

## 🔄 切换模型

- 使用 Qwen（简单解码）：  
  在 `stdafx.h` 中设置 `USE_SIMPLE 1`，并使用 `LLMRunner`。
- 使用 Yi-1.5（带采样）：  
  设置 `USE_SIMPLE 0`，并使用 `LLMRunnerYi`。

---

## 📌 注意事项

- 性能依赖于硬件和模型大小。
- 大模型可能需要 >8GB 显存或在 CPU 上运行较慢。
- 推荐使用量化模型（`Q4_0`、`Q5_K_M` 等）以提升速度。

---

## 📜 许可证

本项目使用 MIT License 许可。  
llama.cpp 使用其原始许可证。

---

## 🙋 致谢

- [ggerganov/llama.cpp](https://github.com/ggerganov/llama.cpp)
- [Qt Project](https://www.qt.io/)


# MiniChatLLM 

A minimal **Qt + llama.cpp** desktop demo for running local Large Language Models (LLMs) in an interactive chat interface.  
Supports **Qwen-1.5** and **Yi-1.5** models (GGUF format) with basic and enhanced sampling.

---

## ✨ Features

- **Local inference** using [llama.cpp](https://github.com/ggerganov/llama.cpp) — no network required.
- **Qt-based GUI** for sending messages and displaying responses.
- **Model loading progress dialog** with elapsed time counter.
- **Two inference backends**:
  - `LLMRunner` — simple greedy decoding.
  - `LLMRunnerYi` — top-p + temperature sampling, tuned for Yi-1.5-6B-Chat.
- **Streaming output** — partial results emitted in real time.
- **Threaded execution** — model runs in a background thread, keeping the UI responsive.
- **Conversation history** — context is maintained across turns.

---

## 📂 Project Structure

```
MiniChatLLM/
├── ChatWindow.h / ChatWindow.cpp     # Main chat UI
├── LLMRunner.h / LLMRunner.cpp       # Qwen/simple LLM backend
├── LLMRunnerYi.h / LLMRunnerYi.cpp   # Yi-1.5-specific backend with sampling
├── LoadingTipWidget.h / .cpp         # Model loading indicator widget
├── stdafx.h / stdafx.cpp             # Common includes & helpers
└── models/                           # Place .gguf models here
```

---

## 🛠 Dependencies

- **C++17** or newer
- [Qt 5.15+](https://www.qt.io/download-qt-installer)
- [llama.cpp (2024.06+)](https://github.com/ggerganov/llama.cpp) built as a static or dynamic library  
- A **GGUF** model file (e.g., Qwen1.5-0.5B-Chat-Q4_0.gguf, Yi-1.5-6B-Chat-Q4_K_M.gguf)

---

## ⚙️ Build Instructions

1. **Clone this repo**  
   ```bash
   git clone https://github.com/yourname/MiniChatLLM.git
   cd MiniChatLLM
   ```

2. **Build llama.cpp** and copy the headers & library into the project:  
   ```bash
   cmake -B build -DLLAMA_BUILD=OFF
   cmake --build build --config Release
   ```
   Place:
   - `llama.h` into `llama/include`
   - `llama.lib` / `llama.dll` (Windows) or `.a` / `.so` (Linux) into `llama/lib`

3. **Open in Qt Creator / Visual Studio**  
   - Ensure the `llama` library path is linked.
   - Set build target to **x64**.
   - Update the model path in `ChatWindow.cpp` to your local `.gguf` file.

4. **Build & run**  
   The first run will load the model (time depends on model size and hardware).

---

## 🚀 Usage

1. Launch the application.
2. Wait for the **"Loading model..."** dialog to finish.
3. Type your question in the input box.
4. Press **Enter** or click **Send**.
5. AI responses will appear in the chat view.

---

## 🔄 Switching Models

- To use Qwen (simple decoding):  
  Set `USE_SIMPLE 1` in `stdafx.h` and use `LLMRunner`.
- To use Yi-1.5 with sampling:  
  Set `USE_SIMPLE 0` and use `LLMRunnerYi`.

---

## 📌 Notes

- Performance depends heavily on hardware and model size.
- Large models may require >8GB VRAM or run slower on CPU.
- For faster responses, use quantized models (`Q4_0`, `Q5_K_M`, etc.).

---

## 📜 License

This project is released under the MIT License.  
llama.cpp is under its respective license.

---

## 🙋 Acknowledgements

- [ggerganov/llama.cpp](https://github.com/ggerganov/llama.cpp)
- [Qt Project](https://www.qt.io/)
