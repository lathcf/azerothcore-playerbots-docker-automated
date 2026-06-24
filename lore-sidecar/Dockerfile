# Lore sidecar: classify -> retrieve (MySQL) -> phrase (Ollama).
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 8091
# Bind host/port come from LORE_LISTEN (default 0.0.0.0:8091).
CMD ["sh", "-c", "uvicorn app:build_default_app --factory --host ${LORE_LISTEN%%:*} --port ${LORE_LISTEN##*:}"]
