FROM alpine

ARG UD3TN_VERSION

RUN apk add git make gcc libc-dev
RUN git clone --recursive --depth=1 --branch $UD3TN_VERSION https://gitlab.com/d3tn/ud3tn.git
RUN cd ud3tn && make posix

FROM alpine
RUN apk add coreutils
COPY --from=0 /ud3tn/build/posix/ud3tn /ud3tn
ENTRYPOINT [ "stdbuf", "-oL", "/ud3tn" ]
