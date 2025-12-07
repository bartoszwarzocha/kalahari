# Workflow Orchestrator

Uruchom orkiestratora workflow dla zadania podanego przez użytkownika.

## Instrukcje

1. Pobierz opis zadania z argumentu: $ARGUMENTS
2. Uruchom orkiestratora Python:

```bash
cd E:\Python\Projekty\Kalahari\.claude && python -m orchestrator.main "$ARGUMENTS"
```

3. Orkiestrator automatycznie:
   - Uruchomi task-manager → architect → implementację → review → testy → zamknięcie
   - Będzie pytał o decyzje w kluczowych momentach
   - Wyświetli podsumowanie na końcu

## Uwagi

- Orkiestrator używa SDK do wywoływania agentów
- Każdy agent musi zakończyć odpowiedź blokiem [WORKFLOW_STATUS]
- W przypadku problemów orkiestrator pyta użytkownika o dalsze kroki
