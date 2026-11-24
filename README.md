# AquaFeeder v2
---

## 1. Architecture

| Composant | Technologie | Rôle |
|---|---|---|
| **Backend** | Spring Boot 3.5 (Java 21), JPA/Hibernate, Flyway, Eclipse Paho | API REST, pont MQTT ↔ base de données, simulateur ESP32 intégré (dev) |
| **Base de données** | PostgreSQL 16 | Télémétrie, repas, alertes, réglages persistés |
| **Frontend** | Next.js 16 (App Router, TypeScript) | Console de monitoring responsive (design néon/glass conservé) |
| **Broker MQTT** | Mosquitto 2 | Transport des messages vers/depuis l'ESP32 réel ou simulé |
| **Orchestration** | Docker Compose | Un seul fichier, deux fichiers d'environnement (dev / prod) |

```text
ESP32 (ou simulateur intégré)
        │  MQTT  shrimp/food/level | shrimp/count | shrimp/threshold | shrimp/alert
        ▼
   Mosquitto ──▶ Spring Boot ──▶ PostgreSQL
        ▲             │  REST /api/*
        │             ▼
        └── commandes Next.js (navigateur)
```
---

## 2. Démarrage rapide

### Prérequis
- Docker ≥ 24 avec le plugin `compose` (ou Docker Desktop)


### Développement
```bash
cd ~/Documents/code/aquafeeder-v2
./scripts/start-dev.sh
```
- Console : http://localhost:3000
- API : http://localhost:8080/api/status
- Santé : http://localhost:8080/actuator/health
- PostgreSQL : `localhost:5432` (utilisateur/mot de passe du `.env.dev`)
- MQTT : `localhost:1883`
- Le simulateur ESP32 est **activé** : le niveau baisse lentement, les repas programmés et les commandes manuelles fonctionnent immédiatement.

### Production
```bash
# 1) Éditer .env.prod : mots de passe, domaines, URL publique de l'API
# 2) Puis :
./scripts/start-prod.sh
```
En prod, le simulateur est désactivé et les ports sont liés à `127.0.0.1` : placez un reverse-proxy (Traefik/Nginx/Caddy) devant les conteneurs et renseignez `CORS_ALLOWED_ORIGINS` + `NEXT_PUBLIC_API_BASE_URL` avec vos domaines publics.

### Commandes utiles
```bash
./scripts/logs.sh .env.dev backend       # logs du backend
./scripts/logs.sh .env.dev frontend      # logs du frontend
./scripts/smoke-test.sh .env.dev         # vérification des endpoints HTTP
./scripts/stop.sh .env.dev               # arrêt de la stack dev
docker compose --env-file .env.dev ps    # état des services
```

---

## 3.Variables d'environnement

Deux fichiers sont fournis et **ignorés par Git** (`.gitignore`) :

| Fichier | Usage |
|---|---|
| `.env.dev` | Développement : simulateur activé, ports exposés, CORS localhost |
| `.env.prod` | Production : simulateur désactivé, secrets à remplacer, binding loopback |
| `.env.example` | Modèle versionné, documente toutes les variables |

Ils sont passés à la fois à `docker compose --env-file` (interpolation des ports/urls/images) et à chaque conteneur via `env_file`. Le backend sait aussi lire ces fichiers lors d'un lancement hors Docker (`SPRING_PROFILES_ACTIVE=dev` → `.env.dev`).

### Groupes de variables

| Groupe | Variables clés |
|---|---|
| Général | `COMPOSE_PROJECT_NAME`, `AQUAFEEDER_APP_NAME`, `SPRING_PROFILES_ACTIVE`, `TZ` |
| PostgreSQL | `POSTGRES_IMAGE`, `POSTGRES_DB`, `POSTGRES_USER`, `POSTGRES_PASSWORD`, `POSTGRES_PORT`, `POSTGRES_HOST_PORT`, `POSTGRES_BIND_IP` |
| MQTT | `MQTT_IMAGE`, `MQTT_PORT`, `MQTT_HOST_PORT`, `MQTT_BIND_IP`, `MQTT_URL`, `MQTT_USER`, `MQTT_PASSWORD`, `MQTT_CLIENT_ID`, `MQTT_TOPIC_PREFIX` |
| Backend | `BACKEND_PORT`, `BACKEND_HOST_PORT`, `BACKEND_BIND_IP`, `CORS_ALLOWED_ORIGINS`, `API_KEY`, `CAPACITY_GRAMS`, `FEED_MIN_INTERVAL_SECONDS`, `HISTORY_RETENTION_DAYS`, `HISTORY_WINDOW_MINUTES`, `RETENTION_CRON` |
| Simulateur | `SIMULATOR_ENABLED`, `SIM_INITIAL_FOOD_GRAMS`, `SIM_INITIAL_SHRIMP_COUNT`, `SIM_LOW_THRESHOLD_GRAMS`, `SIM_FEED_GRAMS`, `SIM_PUBLISH_INTERVAL_MS`, `SIM_SENSOR_DRIFT_MAX_GRAMS`, `SIM_SCHEDULED_HOURS` |
| Frontend | `FRONTEND_PORT`, `FRONTEND_HOST_PORT`, `FRONTEND_BIND_IP`, `NEXT_PUBLIC_API_BASE_URL`, `NEXT_PUBLIC_APP_NAME`, `NEXT_PUBLIC_POLL_INTERVAL_MS`, `NEXT_PUBLIC_FOOD_CAPACITY_GRAMS`, `NEXT_PUBLIC_HISTORY_WINDOW_MINUTES`, `NEXT_PUBLIC_SCHEDULED_HOURS`, `NEXT_PUBLIC_FEED_MIN_INTERVAL_SECONDS` |

> `MQTT_USER` / `MQTT_PASSWORD` vides ⇒ Mosquitto en accès anonyme. Renseignez-les en production pour activer l'authentification (le fichier de mots de passe est généré automatiquement au démarrage du conteneur).
> `API_KEY` vide ⇒ filtre désactivé. Sinon, chaque appel REST doit envoyer l'en-tête `X-API-Key`. Comme le frontend s'exécute dans le navigateur, n'activez ce filtre que si un reverse-proxy injecte l'en-tête côté serveur (sinon l'UI recevra des 401).

---

## 4. API REST

Base : `http://localhost:8080` (dev).

| Méthode | Endpoint | Description |
|---|---|---|
| `GET` | `/api/status` | État courant (niveau, crevettes, seuil, autonomie, santé MQTT, repas récents) |
| `GET` | `/api/history?minutes=10` | Série temporelle du niveau pour le graphe |
| `GET` | `/api/health` | Diagnostic (MQTT, base, uptime) |
| `GET` | `/api/config` | Configuration publique pour l'UI |
| `GET` | `/api/feeds?limit=20` | Historique des distributions |
| `GET` | `/api/alerts?limit=20` | Historique des alertes |
| `POST` | `/api/feed` | Distribution manuelle (`200`, `429` si rate-limit, `503` si MQTT hors ligne) |
| `POST` | `/api/shrimp/count` | `{"count": 25}` |
| `POST` | `/api/threshold` | `{"grams": 15}` |
| `GET` | `/actuator/health` | Healthcheck Spring Boot |

Documentation OpenAPI (si activée) : `/swagger-ui.html`.

### Topics MQTT

Préfixe configurable (`MQTT_TOPIC_PREFIX`, défaut `shrimp`) :

| Topic | Direction | Payload |
|---|---|---|
| `<prefix>/food/level` | ESP32 → API | grammes (ex. `78.4`) |
| `<prefix>/count` | ESP32 → API | entier |
| `<prefix>/threshold` | ESP32 → API | grammes |
| `<prefix>/alert` | ESP32 → API | `low_food` |
| `<prefix>/feed/status` | ESP32 → API | `ok` / `rate_limited` |
| `<prefix>/feed/command` | API → ESP32 | `manual` |
| `<prefix>/count/set` | API → ESP32 | `{"count":N}` |
| `<prefix>/threshold/set` | API → ESP32 | `{"grams":N}` |

---

## 5. Développement local (sans Docker)

```bash
# Backend — lit automatiquement .env.dev
cd backend && ./mvnw spring-boot:run

# Frontend — réplique les NEXT_PUBLIC_* dans frontend/.env.development.local
cd .. && ./scripts/sync-frontend-env.sh dev
cd frontend && docker run --rm -v "$PWD":/app -w /app node:22-alpine npm install
docker run --rm -v "$PWD":/app -w /app -p 3000:3000 node:22-alpine npm run dev
```

---

## 6. Données et schéma

Flyway crée quatre tables : `telemetry`, `feed_event`, `alert_event`, `app_setting`. La rétention est appliquée par une tâche planifiée (`RETENTION_CRON`) qui supprime les enregistrements plus anciens que `HISTORY_RETENTION_DAYS`. Les volumes Docker `pg-data` et `mqtt-data` conservent les données entre redémarrages.

---

## 7. Matériel

Le firmware d'origine est dans [`firmware/ESP32_Feeder.ino`](firmware/ESP32_Feeder.ino). Il publie déjà `shrimp/food/level`, `shrimp/count` et `shrimp/alert`, et écoute `shrimp/feed/command`. Pour profiter des commandes `count/set` et `threshold/set`, il faudra ajouter deux abonnements dans le firmware et remplacer les constantes Wi-Fi/MQTT par un mécanisme de configuration (provisioning). Le reste fonctionne sans modification.

---

*Licence MIT — AquaFeeder, parce que vos crevettes méritent un dîner digne de la meilleure ville côtière.*
