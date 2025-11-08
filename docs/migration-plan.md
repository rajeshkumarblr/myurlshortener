# Migration Plan: Database Schema Initialization

Goal: move schema creation out of the application runtime into a Kubernetes-native process for reliability and least privilege.

## Approach Options

- InitContainer: runs before app container, executes idempotent SQL to ensure schema exists.
- Kubernetes Job: a separate job for migrations; can be re-run safely and tracked independently.

## Recommended: Job-based Migration

Pros: auditable runs, decoupled from app lifecycle, failure surfaced clearly.

Example Job (psql via `DATABASE_URL` secret):

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: url-shortener-migrate
spec:
  template:
    spec:
      restartPolicy: OnFailure
      containers:
        - name: migrate
          image: postgres:16
          env:
            - name: DATABASE_URL
              valueFrom:
                secretKeyRef:
                  name: external-postgres
                  key: DATABASE_URL
          command: ["/bin/sh","-c"]
          args:
            - >-
              psql "$DATABASE_URL" <<'SQL'
              CREATE TABLE IF NOT EXISTS url_mapping (
                code TEXT PRIMARY KEY,
                url TEXT NOT NULL,
                expires_at TIMESTAMPTZ
              );
              SQL
```

## InitContainer Alternative

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: url-shortener
spec:
  template:
    spec:
      initContainers:
        - name: migrate
          image: postgres:16
          env:
            - name: DATABASE_URL
              valueFrom:
                secretKeyRef:
                  name: external-postgres
                  key: DATABASE_URL
          command: ["/bin/sh","-c"]
          args:
            - >-
              psql "$DATABASE_URL" -c "CREATE TABLE IF NOT EXISTS url_mapping (code TEXT PRIMARY KEY, url TEXT NOT NULL, expires_at TIMESTAMPTZ);"
      containers:
        - name: url-shortener
          # ...
```

## Next Steps

1) Add the Job manifest under `k8s/migration-job.yaml` (or embed initContainer into the Deployment).
2) Remove runtime DDL from the app once Job is verified.
3) Enforce least-privileged DB role for the app (no CREATE permissions).
4) Document operational runbook for applying migrations in CI/CD.
