FROM python:3
COPY . /usr/src/a1
WORKDIR /usr/src/a1
RUN make
WORKDIR ./tests
RUN python3 concurrent.py
RUN python3 invalid_login.py
RUN python3 fastforward.py
RUN python3 reconnect.py