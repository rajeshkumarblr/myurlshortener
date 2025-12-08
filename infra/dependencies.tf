resource "helm_release" "redis" {
  name             = "redis"
  chart            = "./charts/redis-24.0.0.tgz"
  namespace        = "cache"
  create_namespace = true

  set {
    name  = "auth.enabled"
    value = "true"
  }
  set {
    name  = "auth.password"
    value = "UrlShortRedis2025"
  }
  set {
    name  = "architecture"
    value = "standalone"
  }
  set {
    name  = "replica.replicaCount"
    value = "0"
  }
  set {
    name  = "master.persistence.size"
    value = "2Gi"
  }
  set {
    name  = "master.resources.requests.memory"
    value = "128Mi"
  }
  set {
    name  = "master.resources.requests.cpu"
    value = "50m"
  }
  set {
    name  = "master.resources.limits.memory"
    value = "256Mi"
  }
  set {
    name  = "master.resources.limits.cpu"
    value = "200m"
  }
}

resource "helm_release" "postgres" {
  name             = "postgres"
  chart            = "./charts/postgresql-18.1.13.tgz"
  namespace        = "default" # Deploying to default as per original script
  create_namespace = true

  set {
    name  = "auth.postgresPassword"
    value = "UrlShortPass2025"
  }
  set {
    name  = "auth.database"
    value = "urlshortener"
  }
  set {
    name  = "primary.persistence.size"
    value = "2Gi"
  }
  set {
    name  = "primary.resources.requests.memory"
    value = "256Mi"
  }
  set {
    name  = "primary.resources.requests.cpu"
    value = "100m"
  }
  set {
    name  = "primary.resources.limits.memory"
    value = "512Mi"
  }
  set {
    name  = "primary.resources.limits.cpu"
    value = "500m"
  }
}
