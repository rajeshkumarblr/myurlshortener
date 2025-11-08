---
name: DB Migration Job
about: Track moving schema creation to a Kubernetes Job/initContainer
labels: enhancement, database
---

## Summary
Move schema creation out of the app into a Kubernetes-native migration (Job or initContainer) and reduce DB privileges for the app role.

## Acceptance Criteria
- A Job or initContainer applies idempotent DDL for `url_mapping`.
- App starts without CREATE permissions.
- Documentation updated (`docs/migration-plan.md`).

## Notes
- See `docs/migration-plan.md` for YAML snippets and next steps.
- Prefer Job for auditability; initContainer acceptable for simplicity.
