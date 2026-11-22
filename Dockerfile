# ---------- Build ----------
FROM maven:3.9-eclipse-temurin-21 AS build
WORKDIR /build
COPY pom.xml ./
COPY mvnw mvnw.cmd ./
COPY .mvn ./.mvn
RUN chmod +x mvnw && mvn -B -q -DskipTests dependency:go-offline
COPY src ./src
RUN mvn -B -q -DskipTests package

# ---------- Runtime ----------
FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S aquafeeder && adduser -S -G aquafeeder aquafeeder
WORKDIR /app
COPY --from=build /build/target/aquafeeder-backend.jar /app/app.jar
USER aquafeeder
EXPOSE 8080
CMD ["java", "-jar", "/app/app.jar"]
