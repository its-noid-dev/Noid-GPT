# NoidGPT 1.3.0

- Open WebUI en WebView2 volledig verwijderd; NoidGPT gebruikt de eigen native Windows-chatinterface.
- Geen aparte Python-runtime, Open WebUI-download of WebView2 SDK/runtime meer nodig.
- Standaardmodel gewijzigd naar `qwen3.5:4b-q4_K_M` (Q4_K_M, ongeveer 3.4 GB modelbestand; actuele taggrootte kan variëren).
- Modelkeuze, redeneerniveau, antwoordtaal en optioneel webzoeken toegevoegd aan de interface.
- Webzoeken gebruikt DuckDuckGo HTML via WinHTTP; de vraag gaat naar DuckDuckGo als Web zoeken aanstaat, en resultaten worden als niet-vertrouwde context aan het lokale model meegegeven.
- Installer ruimt oude Open WebUI/Python/uv/WebView2-componenten op, maar behoudt Ollama-modellen en gebruikersinstellingen.
