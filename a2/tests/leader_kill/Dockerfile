FROM python:latest

WORKDIR /code
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY src/ .
CMD ["python3", "./test_leader_kill.py"]
