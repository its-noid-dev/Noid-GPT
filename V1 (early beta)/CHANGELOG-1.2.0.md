# 1.2.0

- Host Open WebUI inside the existing Win32 application using Microsoft Edge WebView2.
- Bootstrap uv, Python 3.12 and Open WebUI into the NoidGPT data directory on first run.
- Launch Open WebUI bound to 127.0.0.1:8080 and set OLLAMA_BASE_URL to the local Ollama server.
- Use a Windows Job Object with KILL_ON_JOB_CLOSE to stop Open WebUI child processes when NoidGPT exits.
- Fix the GetDiskFreeSpaceExW argument types using ULARGE_INTEGER and resolve the actual data-volume root.
- Download the WebView2 C++ SDK automatically during the Visual Studio build.
