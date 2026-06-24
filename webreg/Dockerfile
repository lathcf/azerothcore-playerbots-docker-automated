# Build a static binary, then ship it on distroless (non-root).
FROM golang:1.22-alpine AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -trimpath -ldflags="-s -w" -o /ac-webreg .

FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=build /ac-webreg /ac-webreg
EXPOSE 8090
USER nonroot:nonroot
ENTRYPOINT ["/ac-webreg"]
