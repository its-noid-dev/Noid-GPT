# NoidGPT 1.3.0

NoidGPT is een native Windows C++ chatapp voor lokale modellen via Ollama. Open WebUI en WebView2 zijn niet meer nodig.

## Aanbevolen model voor 16 GB RAM

Standaard gebruikt NoidGPT `qwen3.5:4b-q4_K_M` (Q4_K_M, ongeveer 3.4 GB download volgens de Ollama modelpagina). Dit is een sterke kleine instructie-/reasoningkeuze die onder 5 GB blijft. De werkelijke geheugenvraag hangt af van contextlengte en andere apps.

Andere keuzes in de modelkeuzelijst: `llama3.1:8b`, `phi4-mini` en `llama3.2:3b`. Een nieuwe modelkeuze wordt opgeslagen en wordt actief na herstart; het gekozen model wordt dan zo nodig gedownload.

## Functies

- Native Win32-interface; geen Open WebUI, Python of WebView2.
- Keuze van model.
- Redeneerniveau: Snel, Normaal, Grondig of Diep nadenken. Dit past de instructies en tokenlimiet aan; de precieze werking hangt ook af van het gekozen model.
- Antwoordtaal: Automatisch, Nederlands, English, Français of Deutsch.
- Optioneel webzoeken via DuckDuckGo wanneer internet beschikbaar is. Als de optie aanstaat, wordt je vraag naar DuckDuckGo gestuurd om zoekresultaten op te halen; die resultaten worden als context aan het lokale model gegeven. Het model zelf blijft lokaal draaien. Zet Web zoeken uit voor volledig lokaal gebruik.
- Lokale chat via Ollama, streaming antwoorden en annuleren.

## Bouwen

Vereist Visual Studio met de Desktop development with C++ workload en Windows SDK. Open `NoidGPT.slnx`, kies `Release | x64` en bouw. Of voer `Build-Release.ps1` uit; Inno Setup 6 is nodig voor de installer.

De eerste start downloadt Ollama en het gekozen model. Webzoeken vereist een internetverbinding; zonder internet werkt lokale chat nog steeds.
