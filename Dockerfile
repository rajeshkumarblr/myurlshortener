# Stage 1: Build Frontend
FROM node:18-alpine AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package*.json ./
RUN npm install
COPY frontend/ .
# Override output directory to ./dist for Docker build
RUN npm run build -- --outDir dist

# Stage 2: Build Backend
FROM maven:3.9-eclipse-temurin-21 AS backend-builder
WORKDIR /app
COPY pom.xml .
# Download dependencies first (cached layer)
RUN mvn dependency:go-offline
COPY src ./src
# Copy built frontend assets to Spring Boot static folder
COPY --from=frontend-builder /app/frontend/dist ./src/main/resources/static
RUN mvn clean package -DskipTests

# Stage 3: Runtime
FROM eclipse-temurin:21-jre-jammy
WORKDIR /app
RUN groupadd -r appuser && useradd -r -g appuser appuser
COPY --from=backend-builder /app/target/*.jar app.jar
USER appuser
EXPOSE 9090
CMD ["java", "-jar", "app.jar"]
