
# Use an official Ubuntu base image
FROM ubuntu:25.04
# Set working directory
WORKDIR /app

# Copy local files into the image
# Install dependencies
RUN apt-get update && apt-get install -y gdbserver docker.io libprotobuf-dev protobuf-compiler libssl-dev


COPY ../bin/DebugDocker/God/God /app/
# For GDBserver
EXPOSE 1234 
# Set default command
CMD ["gdbserver", ":1234","God"]

