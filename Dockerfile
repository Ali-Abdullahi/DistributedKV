FROM alpine:3.19 AS build
RUN apk add --no-cache gcc musl-dev make
WORKDIR /src
COPY Makefile *.c *.h ./
RUN make

FROM alpine:3.19
COPY --from=build /src/kv /usr/local/bin/kv
WORKDIR /data
ENTRYPOINT ["kv"]
