# NoidGPT 1.2.4

- Fixed missing WebView2 SDK include path in Release x64 and Release Win32 configurations.
- SDK bootstrap now validates the extracted header and x64 loader library and reports the exact failing path.
- Build checks WebView2 SDK immediately before compilation.
- Installer performs a clean reinstall of NoidGPT-managed runtimes (Open WebUI virtual environment, uv, managed Python runtime/cache, and the app-downloaded Ollama binaries).
- User data is preserved: Ollama models, Open WebUI database/chats, configuration, and WebView2 profile.
