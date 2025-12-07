# Workflow Orchestrator (Mock Mode)

Uruchom orkiestratora workflow w trybie testowym (bez prawdziwych wywołań agentów).

## Instrukcje

1. Pobierz opis zadania z argumentu: $ARGUMENTS
2. Uruchom orkiestratora w trybie mock:

```bash
cd E:\Python\Projekty\Kalahari\.claude && python -m orchestrator.main --mock "$ARGUMENTS"
```

3. Tryb mock symuluje odpowiedzi agentów - służy do testowania flow bez kosztów API.
