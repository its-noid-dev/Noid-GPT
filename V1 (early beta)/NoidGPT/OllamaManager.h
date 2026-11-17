#pragma once

#include "framework.h"
#include <winhttp.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Beheert Ollama: zoeken/downloaden, starten, model ophalen en chatten (streaming).
// Alle callbacks worden vanuit een achtergrondthread aangeroepen.
class OllamaManager
{
public:
    struct Callbacks
    {
        std::function<void(const std::wstring&)> status;        // statustekst
        std::function<void(int)>                  progress;     // 0-100, -1 = verbergen
        std::function<void(const std::wstring&)>  token;        // stukje antwoord
        std::function<void(bool, const std::wstring&)> chatDone; // ok, foutmelding
        std::function<void(bool)>                 ready;        // klaar voor gebruik?
    };

    explicit OllamaManager(Callbacks cb);
    ~OllamaManager();

    void Start();                                // asynchroon: Ollama + model klaarzetten
    bool SendUserMessage(const std::string& utf8); // start streaming antwoord
    void Cancel();                               // stop huidige actie
    void ResetChat();                            // wis gesprekscontext
    void SetChatOptions(const std::string& language, const std::string& effort, bool webSearch);
    void Shutdown();

    bool IsBusy() const { return m_chatBusy.load(); }
    const std::wstring& Model() const { return m_modelW; }
    std::wstring DataDir() const { return m_dataDir; }
    std::wstring ConfigFile() const { return m_iniPath; }
    bool IsPortable() const { return m_portable; }

private:
    struct Url { std::wstring host; INTERNET_PORT port = 80; std::wstring path; bool https = false; };
    struct HttpResult { int status = 0; unsigned long long contentLength = 0; };
    using DataFn = std::function<bool(const char*, size_t, unsigned long long)>;

    HttpResult Http(const wchar_t* method, const Url& url, const std::string& body,
                    const DataFn& onData, std::string* errBody, DWORD recvTimeoutMs);
    static Url LocalUrl(const wchar_t* path);

    void InitWorker();
    void ChatWorker();
    bool PingServer();
    bool HasModel();
    bool PullModel();
    bool DownloadOllama();
    bool StartServer(const std::wstring& exe);
    std::wstring FindOllama() const;
    void CloseCurrent();

    Callbacks m_cb;
    std::string m_model;
    std::wstring m_modelW;
    std::wstring m_exeDir, m_dataDir, m_iniPath;
    bool m_portable = false;

    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_chatBusy{false};
    std::atomic<HINTERNET> m_cur{nullptr};
    std::thread m_initThread, m_chatThread;

    std::mutex m_settingsMutex;
    std::string m_language = "Automatisch";
    std::string m_effort = "Normaal";
    bool m_webSearch = true;

    std::mutex m_histMutex;
    std::vector<std::pair<std::string, std::string>> m_history; // role, content (UTF-8)

    HANDLE m_job = nullptr;
    HANDLE m_serverProcess = nullptr;
};
